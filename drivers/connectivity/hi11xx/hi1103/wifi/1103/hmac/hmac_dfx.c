


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif


/*****************************************************************************
  1 头文件包含
*****************************************************************************/
#include "oal_ext_if.h"
#include "oam_ext_if.h"
#include "wlan_spec.h"
#include "hmac_dfx.h"
#include "hmac_ext_if.h"

#ifdef _PRE_WLAN_1103_CHR
#include "mac_resource.h"
#include "mac_vap.h"
#include "chr_user.h"
#include "chr_errno.h"
#endif

#undef  THIS_FILE_ID
#define THIS_FILE_ID OAM_FILE_ID_HMAC_DFX_C
/*****************************************************************************
  2 全局变量定义
*****************************************************************************/
#ifdef _PRE_WLAN_FEATURE_DFR
hmac_dfr_info_stru  g_st_dfr_info;         /* DFR异常复位开关 */
#endif

OAL_STATIC oam_cfg_data_stru  g_ast_cfg_data[OAM_CFG_TYPE_BUTT] =
{
    {OAM_CFG_TYPE_MAX_ASOC_USER,     "USER_SPEC",     "max_asoc_user",     31},
};

#if defined(_PRE_PRODUCT_ID_HI110X_HOST)
#if (_PRE_MULTI_CORE_MODE_OFFLOAD_DMAC == _PRE_MULTI_CORE_MODE)
/* 由于WL_L2_DRAM大小限制，目前暂时开放2个业务vap，整体规格开放待后续优化 TBD */
oal_uint32   g_ul_wlan_vap_max_num_per_device = 4 + 1;  /* 4个AP + 1个配置vap */
#else
oal_uint32   g_ul_wlan_vap_max_num_per_device = 4 + 1;  /* 4个AP + 1个配置vap */
#endif

#else
extern oal_uint32   g_ul_wlan_vap_max_num_per_device;
#endif

/*****************************************************************************
  3 函数实现  TBD 置换函数名称
*****************************************************************************/

oal_int32  oam_cfg_get_item_by_id(oam_cfg_type_enum_uint16  en_cfg_type)
{
    oal_uint32      ul_loop;

    for (ul_loop = 0; ul_loop < OAM_CFG_TYPE_BUTT; ul_loop++)
    {
        if (en_cfg_type == g_ast_cfg_data[ul_loop].en_cfg_type)
        {
            break;
        }
    }

    if (OAM_CFG_TYPE_BUTT == ul_loop)
    {
        OAL_IO_PRINT("oam_cfg_get_item_by_id::get cfg item failed!\n");
        return -OAL_FAIL;
    }

    return g_ast_cfg_data[ul_loop].l_val;
}


OAL_STATIC oal_uint32  oam_cfg_restore_all_item(oal_int32 al_default_cfg_data[])
{
    oal_uint32          ul_loop;

    for (ul_loop = 0; ul_loop < OAM_CFG_TYPE_BUTT; ul_loop++)
    {
        g_ast_cfg_data[ul_loop].l_val = al_default_cfg_data[ul_loop];
    }

    return OAL_SUCC;
}



oal_int32  oam_cfg_get_all_item(oal_void)
{
    oal_int32     al_default_cfg_data[OAM_CFG_TYPE_BUTT] = {0};
    oal_uint8    *puc_plaintext;
    oal_uint8    *puc_ciphertext;
    oal_uint32    ul_file_size = 0;
    oal_uint32    ul_loop;
    oal_int32     l_ret;
    oal_int       i_key[OAL_AES_KEYSIZE_256] = {0x1,0x2,0x3,0x4d,0x56,0x10,0x11,0x12,
                                                0x1,0x2,0x3,0x4d,0x56,0x10,0x11,0x12,
                                                0x1,0x2,0x3,0x4d,0x56,0x10,0x11,0x12,
                                                0x1,0x2,0x3,0x4d,0x56,0x10,0x11,0x12};

    oal_aes_key_stru    st_aes_key;

    /* 保存默认配置，如果获取配置文件中信息的时候中间有失败的情况，则需要恢复
       前面全局配置信息，其它模块加载的时候可以按照默认配置加载
    */
    for (ul_loop = 0; ul_loop < OAM_CFG_TYPE_BUTT; ul_loop++)
    {
        al_default_cfg_data[ul_loop] = g_ast_cfg_data[ul_loop].l_val;
    }

    /* 获取文件大小并获取文件指针 */
    l_ret = oal_file_size(&ul_file_size);
    if (OAL_SUCC != l_ret)
    {
        OAL_IO_PRINT("oam_cfg_get_all_item::get file size failed!\n");
        return l_ret;
    }

    /* 将配置文件中的所有数据读到一个缓冲区里，此时数据是加密的 */
    puc_ciphertext = oal_memalloc(ul_file_size + OAM_CFG_STR_END_SIGN_LEN);
    if (OAL_PTR_NULL == puc_ciphertext)
    {
        OAL_IO_PRINT("oam_cfg_get_all_item::alloc ciphertext buf failed! load ko with default cfg!\n");
        return OAL_ERR_CODE_PTR_NULL;
    }
    OAL_MEMZERO(puc_ciphertext, ul_file_size + OAM_CFG_STR_END_SIGN_LEN);

    l_ret = oam_cfg_read_file_to_buf((oal_int8 *)puc_ciphertext, ul_file_size);
    if (OAL_SUCC != l_ret)
    {
        OAL_IO_PRINT("oam_cfg_get_all_item::get cfg data from file failed! fail id-->%d\n", l_ret);
        oal_free(puc_ciphertext);
        return l_ret;
    }

    /* 申请明文空间，并将密文解密 */
    puc_plaintext = oal_memalloc(ul_file_size + OAM_CFG_STR_END_SIGN_LEN);
    if (OAL_PTR_NULL == puc_plaintext)
    {
        OAL_IO_PRINT("oam_cfg_get_all_item::alloc pc_plaintext buf failed! load ko with default cfg!\n");
        oal_free(puc_ciphertext);

        return OAL_ERR_CODE_PTR_NULL;
    }
    OAL_MEMZERO(puc_plaintext, ul_file_size + OAM_CFG_STR_END_SIGN_LEN);

    /* 解密 */
    l_ret = (oal_int32)oal_aes_expand_key(&st_aes_key,(oal_uint8 *)i_key,OAL_AES_KEYSIZE_256);
    if (OAL_SUCC != l_ret)
    {
        oal_free(puc_plaintext);
        oal_free(puc_ciphertext);

        return l_ret;
    }

    oam_cfg_decrypt_all_item(&st_aes_key, (oal_int8 *)puc_ciphertext,
                            (oal_int8 *)puc_plaintext, ul_file_size);

    /* 获取配置文件中每一项的信息，保存到OAM内部结构中 */
    for (ul_loop = 0; ul_loop < OAM_CFG_TYPE_BUTT; ul_loop++)
    {
        l_ret = oam_cfg_get_one_item((oal_int8 *)puc_plaintext,
                                     g_ast_cfg_data[ul_loop].pc_section,
                                     g_ast_cfg_data[ul_loop].pc_key,
                                     &g_ast_cfg_data[ul_loop].l_val);

        /* 如果获取某一配置值不成功，则恢复配置项的默认值 */
        if (OAL_SUCC != l_ret)
        {
            OAL_IO_PRINT("oam_cfg_get_all_item::get cfg item fail! ul_loop=%d\n", ul_loop);

            oam_cfg_restore_all_item(al_default_cfg_data);
            oal_free(puc_plaintext);
            oal_free(puc_ciphertext);

            return l_ret;
        }
    }

    /* 释放缓冲区 */
    oal_free(puc_plaintext);
    oal_free(puc_ciphertext);

    return OAL_SUCC;
}



oal_uint32  oam_cfg_init(oal_void)
{
    oal_int32      l_ret = OAL_SUCC;

    l_ret = oam_cfg_get_all_item();

    return (oal_uint32)l_ret;
}

oal_uint32 hmac_dfx_init(void)
{
    oam_register_init_hook(OM_WIFI, oam_cfg_init);
    return OAL_SUCC;
}

oal_uint32 hmac_dfx_exit(void)
{
    return OAL_SUCC;
}

#ifdef _PRE_WLAN_1103_CHR

oal_uint32 hmac_chr_get_vap_info(mac_vap_stru *pst_mac_vap, hmac_chr_vap_info_stru *pst_vap_info)
{
    mac_user_stru     *pst_mac_user;
    mac_device_stru   *pst_mac_device;
    oal_uint8          uc_vap_idx;
    mac_vap_stru      *pst_mac_vap_tmp  = OAL_PTR_NULL;

    pst_mac_device = mac_res_get_dev(0);
    if (OAL_PTR_NULL == pst_mac_device)
    {
       OAM_ERROR_LOG0(0, OAM_SF_ANY, "{hmac_chr_get_vap_info: mac device is null ptr.}");
       return OAL_ERR_CODE_PTR_NULL;
    }

    pst_vap_info->uc_vap_state  = pst_mac_vap->en_vap_state;
    pst_vap_info->uc_vap_num    = pst_mac_device->uc_vap_num;
    pst_vap_info->uc_vap_rx_nss = pst_mac_vap->en_vap_rx_nss;
    pst_vap_info->uc_protocol   = pst_mac_vap->en_protocol;
    oal_memcopy(&pst_vap_info->st_channel, &pst_mac_vap->st_channel, OAL_SIZEOF(pst_mac_vap->st_channel));

    /*sta 关联的AP的能力*/
    pst_mac_user = mac_res_get_mac_user(pst_mac_vap->us_assoc_vap_id);
    if (OAL_PTR_NULL != pst_mac_user)
    {
        pst_vap_info->uc_ap_spatial_stream_num = pst_mac_user->en_user_num_spatial_stream;
        pst_vap_info->bit_ap_11ntxbf           = pst_mac_user->st_cap_info.bit_11ntxbf;
        pst_vap_info->bit_ap_qos               = pst_mac_user->st_cap_info.bit_qos;
        pst_vap_info->bit_ap_1024qam_cap       = pst_mac_user->st_cap_info.bit_1024qam_cap;
        pst_vap_info->uc_ap_protocol_mode      = pst_mac_user->en_protocol_mode;
    }

    pst_vap_info->bit_ampdu_active    = mac_mib_get_CfgAmpduTxAtive(pst_mac_vap);
    pst_vap_info->bit_amsdu_active    = mac_mib_get_AmsduAggregateAtive(pst_mac_vap);
    pst_vap_info->bit_sta_11ntxbf     = pst_mac_vap->st_cap_flag.bit_11ntxbf;
    pst_vap_info->bit_is_dbac_running = mac_is_dbac_running(pst_mac_device);
    pst_vap_info->bit_is_dbdc_running = mac_is_dbdc_running(pst_mac_device);

    for (uc_vap_idx = 0; uc_vap_idx < pst_mac_device->uc_vap_num; uc_vap_idx++)
    {
        pst_mac_vap_tmp = (mac_vap_stru *)mac_res_get_mac_vap(pst_mac_device->auc_vap_id[uc_vap_idx]);
        if ((OAL_PTR_NULL == pst_mac_vap_tmp) || (MAC_VAP_INVAILD == pst_mac_vap_tmp->uc_init_flag))
        {
            continue;
        }

        if(!IS_LEGACY_VAP(pst_mac_vap_tmp))
        {
            pst_vap_info->st_p2p_info.uc_vap_mode       = pst_mac_vap_tmp->en_vap_mode;
            pst_vap_info->st_p2p_info.uc_vap_state      = pst_mac_vap_tmp->en_vap_state;
            pst_vap_info->st_p2p_info.uc_p2p_mode       = pst_mac_vap_tmp->en_p2p_mode;
            pst_vap_info->st_p2p_info.us_user_nums      = pst_mac_vap_tmp->us_user_nums;
            pst_vap_info->st_p2p_info.uc_chnl_band      = pst_mac_vap_tmp->st_channel.en_band;
            pst_vap_info->st_p2p_info.uc_chnl_bandwidth = pst_mac_vap_tmp->st_channel.en_bandwidth;
            pst_vap_info->st_p2p_info.uc_chnl_idx       = pst_mac_vap_tmp->st_channel.uc_chan_idx;
            pst_vap_info->st_p2p_info.uc_protocol       = pst_mac_vap_tmp->en_protocol;
        }
    }

    return OAL_SUCC;
}

/*去关联共有7种原因(0~6),默认值设置为7，表示没有去关联触发*/
hmac_chr_disasoc_reason_stru g_hmac_chr_disasoc_reason = {0, DMAC_DISASOC_MISC_BUTT};
/*关联字码, 新增4中私有定义字码5200-5203*/
oal_uint16 g_hmac_chr_connect_code[WLAN_VAP_SUPPORT_MAX_NUM_LIMIT] = {0};
/*打点*/
oal_void hmac_chr_set_disasoc_reason(oal_uint16 user_id, oal_uint16 reason_id)
{
    hmac_chr_disasoc_reason_stru *pst_disasoc_reason = OAL_PTR_NULL;

    pst_disasoc_reason = hmac_chr_disasoc_reason_get_pointer();

    pst_disasoc_reason->us_user_id = user_id;
    pst_disasoc_reason->en_disasoc_reason = (dmac_disasoc_misc_reason_enum)reason_id;

    return;
}

/*获取*/
oal_void hmac_chr_get_disasoc_reason(hmac_chr_disasoc_reason_stru *pst_disasoc_reason)
{
    hmac_chr_disasoc_reason_stru *pst_disasoc_reason_temp = OAL_PTR_NULL;

    pst_disasoc_reason_temp = hmac_chr_disasoc_reason_get_pointer();

    pst_disasoc_reason->us_user_id = pst_disasoc_reason_temp->us_user_id;
    pst_disasoc_reason->en_disasoc_reason = pst_disasoc_reason_temp->en_disasoc_reason;

    return;
}

hmac_chr_ba_info_stru g_hmac_chr_ba_info = {0, 0, MAC_UNSPEC_REASON};

/*打点*/
/*梳理删减聚合的流程 计数统计*/
oal_void hmac_chr_set_ba_info(oal_uint8 uc_tid, oal_uint16 reason_id)
{
    hmac_chr_ba_info_stru *pst_ba_info = OAL_PTR_NULL;

    pst_ba_info = hmac_chr_ba_info_get_pointer();
    
    pst_ba_info->uc_del_ba_tid = uc_tid;
    pst_ba_info->en_del_ba_reason = (mac_reason_code_enum)reason_id;

    return;
}

/*获取*/
oal_void hmac_chr_get_ba_info(mac_vap_stru *pst_mac_vap, hmac_chr_ba_info_stru *pst_del_ba_reason)
{   
    hmac_chr_ba_info_stru *pst_ba_info = OAL_PTR_NULL;

    pst_ba_info = hmac_chr_ba_info_get_pointer();
    
    pst_del_ba_reason->uc_ba_num = mac_mib_get_TxBASessionNumber(pst_mac_vap);
    pst_del_ba_reason->uc_del_ba_tid = pst_ba_info->uc_del_ba_tid;
    pst_del_ba_reason->en_del_ba_reason = pst_ba_info->en_del_ba_reason;

    return;
}

oal_uint32  hmac_chr_get_web_fail_slow_info(oal_void)
{
    oal_uint8                uc_vap_index;
    oal_uint8                uc_up_vap_num;
    mac_vap_stru             *pst_mac_vap = OAL_PTR_NULL;
    mac_device_stru          *pst_mac_device = OAL_PTR_NULL;
    hmac_chr_disasoc_reason_stru st_disasoc_reason  = {0};

    pst_mac_device = mac_res_get_dev(0);
    uc_up_vap_num = mac_device_get_up_vap_num(pst_mac_device);

    for (uc_vap_index = 0; uc_vap_index < uc_up_vap_num; uc_vap_index++)
    {
        pst_mac_vap = (mac_vap_stru *)mac_res_get_mac_vap(pst_mac_device->auc_vap_id[uc_vap_index]);
        if(OAL_PTR_NULL == pst_mac_vap)
        {
            OAM_ERROR_LOG0(pst_mac_device->uc_cfg_vap_id, OAM_SF_ANY, "{hmac_chr_get_web_fail_slow_info::cfg_vap is null.}");
            return OAL_ERR_CODE_PTR_NULL;
        }

        if(IS_LEGACY_STA(pst_mac_vap))
        {
            /*原子接口*/
            hmac_chr_get_disasoc_reason(&st_disasoc_reason);
            CHR_EXCEPTION_P(909002024, (oal_uint8 *)(&st_disasoc_reason), OAL_SIZEOF(hmac_chr_disasoc_reason_stru));
        }
    }

    return OAL_SUCC;
}

hmac_chr_disasoc_reason_stru* hmac_chr_disasoc_reason_get_pointer(void)
{
    return &g_hmac_chr_disasoc_reason;
}

oal_void hmac_chr_set_connect_code(oal_uint8 uc_vap_id, oal_uint16 connect_code)
{
    if(uc_vap_id < WLAN_VAP_SUPPORT_MAX_NUM_LIMIT)
    {
        g_hmac_chr_connect_code[uc_vap_id] = connect_code;
    }

    return;
}

oal_uint16* hmac_chr_connect_code_get_pointer(oal_uint8 uc_vap_id)
{
    if(uc_vap_id < WLAN_VAP_SUPPORT_MAX_NUM_LIMIT)
    {
        return &g_hmac_chr_connect_code[uc_vap_id];
    }
    return OAL_PTR_NULL;
}

oal_uint32  hmac_chr_get_connect_info(oal_void)
{
    oal_uint8                uc_vap_index;
    oal_uint8                uc_up_vap_num;
    mac_vap_stru             *pst_mac_vap = OAL_PTR_NULL;
    mac_device_stru          *pst_mac_device = OAL_PTR_NULL;
    oal_uint16               *pst_connect_code   = OAL_PTR_NULL;

    pst_mac_device = mac_res_get_dev(0);
    uc_up_vap_num = mac_device_get_up_vap_num(pst_mac_device);

    for (uc_vap_index = 0; uc_vap_index < uc_up_vap_num; uc_vap_index++)
    {
        pst_mac_vap = (mac_vap_stru *)mac_res_get_mac_vap(pst_mac_device->auc_vap_id[uc_vap_index]);
        if(OAL_PTR_NULL == pst_mac_vap)
        {
            OAM_ERROR_LOG0(pst_mac_device->uc_cfg_vap_id, OAM_SF_ANY, "{hmac_chr_get_connect_info::cfg_vap is null.}");
            return OAL_ERR_CODE_PTR_NULL;
        }

        if(IS_LEGACY_STA(pst_mac_vap))
        {
            /*原子接口*/
            pst_connect_code = hmac_chr_connect_code_get_pointer(pst_mac_vap->uc_vap_id);
            if(OAL_PTR_NULL != pst_connect_code)
            {
                CHR_EXCEPTION_P(909002024, (oal_uint8 *)pst_connect_code, OAL_SIZEOF(oal_uint16));
            }
        }
    }

    return OAL_SUCC;
}
    
hmac_chr_ba_info_stru* hmac_chr_ba_info_get_pointer(void)
{
    return &g_hmac_chr_ba_info;
}
#endif

#ifdef _PRE_WLAN_FEATURE_DFR
oal_module_symbol(g_st_dfr_info);
#endif

#ifdef __cplusplus
    #if __cplusplus
        }
    #endif
#endif
