/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ExynosDisplayFbInterfaceModule.h"

decon_idma_type getDPPChannel(ExynosMPP *otfMPP) {
    if (otfMPP == NULL)
        return MAX_DECON_DMA_TYPE;

    for (int i=0; i < MAX_DECON_DMA_TYPE; i++){
        if((IDMA_CHANNEL_MAP[i].type == otfMPP->mPhysicalType) &&
           (IDMA_CHANNEL_MAP[i].index == otfMPP->mPhysicalIndex))
            return IDMA_CHANNEL_MAP[i].channel;
    }
    return MAX_DECON_DMA_TYPE;
}

//////////////////////////////////////////////////// ExynosPrimaryDisplayFbInterfaceModule //////////////////////////////////////////////////////////////////
ExynosPrimaryDisplayFbInterfaceModule::ExynosPrimaryDisplayFbInterfaceModule(ExynosDisplay *exynosDisplay)
: ExynosPrimaryDisplayFbInterface(exynosDisplay)
{
}

ExynosPrimaryDisplayFbInterfaceModule::~ExynosPrimaryDisplayFbInterfaceModule()
{
}

decon_idma_type ExynosPrimaryDisplayFbInterfaceModule::getDeconDMAType(ExynosMPP *otfMPP)
{
    return getDPPChannel(otfMPP);
}

void ExynosPrimaryDisplayFbInterfaceModule::getDisplayConfigsFromDPU()
{
    struct decon_disp_info disp_info;
    disp_info.ver = HWC_2_0;
    if (ioctl(mDisplayFd, EXYNOS_DISP_INFO, &disp_info) >= 0) {
        for (uint32_t i = 0; i < disp_info.mres_info.mres_number; i++) {
            displayConfigs_t configs;
            configs.vsyncPeriod = mExynosDisplay->mVsyncPeriod; // legacy only supports one refreshrate
            configs.width = disp_info.mres_info.res_info[i].width;
            configs.height = disp_info.mres_info.res_info[i].height;

            configs.Xdpi = 1000 * (mExynosDisplay->mXres * 25.4f) / configs.width;
            configs.Ydpi = 1000 * (mExynosDisplay->mYres * 25.4f) / configs.width;

            configs.cpuIDs = 0;
            for (int cl_num = 0; cl_num < CPU_CLUSTER_CNT; cl_num++)
                configs.minClock[cl_num] = 0;

            ALOGI("Display config : %d, vsync : %d, width : %d, height : %d, xdpi : %d, ydpi : %d",
                    i, configs.vsyncPeriod, configs.width, configs.height, configs.Xdpi, configs.Ydpi);

            mExynosDisplay->mDisplayConfigs.insert(std::make_pair(i,configs));
        }
    }
}

int32_t ExynosPrimaryDisplayFbInterfaceModule::setActiveConfig(hwc2_config_t config)
{
    int32_t ret = NO_ERROR;
    if ((mExynosDisplay->mDisplayConfigs.size() <= 1) && (config != 0))
        ret = HWC2_ERROR_BAD_CONFIG;
    else if ((mExynosDisplay->mDisplayConfigs.size() == 1) && (config == 0))
        ret = HWC2_ERROR_NONE;
    else {

        displayConfigs_t _config = mExynosDisplay->mDisplayConfigs[config];
        struct decon_win_config_data win_data;
        struct decon_win_config *config = win_data.config;
        memset(&win_data, 0, sizeof(win_data));

        config[0].state = decon_win_config::DECON_WIN_STATE_COLOR;

        config[DECON_WIN_UPDATE_IDX].state = decon_win_config::DECON_WIN_STATE_MRESOL;
        config[DECON_WIN_UPDATE_IDX].dst.f_w = _config.width;
        config[DECON_WIN_UPDATE_IDX].dst.f_h = _config.height;
        config[DECON_WIN_UPDATE_IDX].plane_alpha = (int)(1000000000 / _config.vsyncPeriod);

        if ((ret = ioctl(mDisplayFd, S3CFB_WIN_CONFIG, &win_data)) < 0) {
            ALOGE("S3CFB_WIN_CONFIG failed errno : %d", errno);
            return HWC2_ERROR_BAD_CONFIG;
        }

        mExynosDisplay->mXres = config[DECON_WIN_UPDATE_IDX].dst.f_w;
        mExynosDisplay->mYres = config[DECON_WIN_UPDATE_IDX].dst.f_h;
        ALOGI("Display config changed to : %dx%d, %dms, %d Xdpi, %d Ydpi",
                mExynosDisplay->mXres, mExynosDisplay->mYres,
                mExynosDisplay->mVsyncPeriod,
                mExynosDisplay->mXdpi, mExynosDisplay->mYdpi);
        mExynosDisplay->setGeometryChanged(GEOMETRY_DISPLAY_RESOLUTION_CHANGED);
    }

    mActiveConfig = config;

    return ret;
}

//////////////////////////////////////////////////// ExynosExternalDisplayFbInterfaceModule //////////////////////////////////////////////////////////////////
ExynosExternalDisplayFbInterfaceModule::ExynosExternalDisplayFbInterfaceModule(ExynosDisplay *exynosDisplay)
: ExynosExternalDisplayFbInterface(exynosDisplay)
{
}

ExynosExternalDisplayFbInterfaceModule::~ExynosExternalDisplayFbInterfaceModule()
{
}

decon_idma_type ExynosExternalDisplayFbInterfaceModule::getDeconDMAType(ExynosMPP *otfMPP)
{
    return getDPPChannel(otfMPP);
}
