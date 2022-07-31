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
    int32_t num = 0;
    decon_display_mode mode;

    if (ioctl(mDisplayFd, EXYNOS_GET_DISPLAY_MODE_NUM, &num) < 0) {
        ALOGI("Not support EXYNOS_GET_DISPLAY_MODE_NUM : %s", strerror(errno));
        goto use_legacy;
    }

    if (num == 0)
        goto use_legacy;

    for (uint32_t i = 0; i < num; i++) {
        mode.index = i;
        if (ioctl(mDisplayFd, EXYNOS_GET_DISPLAY_MODE, &mode) < 0) {
            ALOGI("Not support EXYNOS_GET_DISPLAY_MODE: index(%d) %s", i, strerror(errno));
            goto use_legacy;
        }
        displayConfigs_t configs;
        configs.vsyncPeriod = 1000000000 / mode.fps;
        configs.width = mode.width;
        configs.height = mode.height;

        configs.Xdpi = 1000 * (mode.width * 25.4f) / mode.mm_width;
        configs.Ydpi = 1000 * (mode.height * 25.4f) / mode.mm_height;

        // Update dpi values for native display mode with real ones
        if (configs.width == mExynosDisplay->mXres && configs.height == mExynosDisplay->mYres) {
            mExynosDisplay->mXdpi = configs.Xdpi;
            mExynosDisplay->mYdpi = configs.Ydpi;
        }

        /* TODO : add cpu affinity/clock settings here */
        configs.cpuIDs = 0;
        for(int cl_num = 0; cl_num < CPU_CLUSTER_CNT; cl_num++)
            configs.minClock[cl_num] = 0;

        ALOGI("Display config : %d, vsync : %d, width : %d, height : %d, xdpi : %d, ydpi : %d",
                i, configs.vsyncPeriod, configs.width, configs.height, configs.Xdpi, configs.Ydpi);

        mExynosDisplay->mDisplayConfigs.insert(std::make_pair(i,configs));
    }

    return;

use_legacy:
    displayConfigs_t configs;
    configs.vsyncPeriod = mExynosDisplay->mVsyncPeriod;
    configs.width = mExynosDisplay->mXres;
    configs.height = mExynosDisplay->mYres;
    configs.Xdpi = mExynosDisplay->mXdpi;
    configs.Ydpi = mExynosDisplay->mYdpi;
    configs.cpuIDs = 0;
    for(int cl_num = 0; cl_num < CPU_CLUSTER_CNT; cl_num++)
        configs.minClock[cl_num] = 0;
    mExynosDisplay->mDisplayConfigs.insert(std::make_pair(0,configs));

    return;
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

        ALOGI(" DARIO DARIO DARIO Display config is : %dx%d, %dms, %d Xdpi, %d Ydpi",
                mExynosDisplay->mXres, mExynosDisplay->mYres,
                mExynosDisplay->mVsyncPeriod,
                mExynosDisplay->mXdpi, mExynosDisplay->mYdpi);

        ALOGI(" DARIO DARIO DARIO requested display config is : %dx%d, %dms, %d Xdpi, %d Ydpi",
                _config.width, _config.height,
                _config.vsyncPeriod,
                mExynosDisplay->mXdpi, mExynosDisplay->mYdpi);

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
        ALOGI("DARIO DARIO DARIO Display config changed to : %dx%d, %dms, %d Xdpi, %d Ydpi",
                mExynosDisplay->mXres, mExynosDisplay->mYres,
                mExynosDisplay->mVsyncPeriod,
                mExynosDisplay->mXdpi, mExynosDisplay->mYdpi);
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
