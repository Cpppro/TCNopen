/**********************************************************************************************************************/
/**
 * @file            tau_tti_types.h
 *
 * @brief           TRDP utility interface definitions
 *
 * @details         This module provides the interface to the following utilities
 *                  - train topology information access type definitions acc. to IEC61375-2-3
 *
 *
 * @note            Project: TCNOpen TRDP prototype stack
 *
 * @author          Armin-H. Weiss (initial version)
 *
 * @remarks This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 *          If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *          Copyright Bombardier Transportation Inc. or its subsidiaries and others, 2014. All rights reserved.
 *
 * $Id$
 *
 */

#ifndef TAU_TTI_TYPES_H
#define TAU_TTI_TYPES_H

/***********************************************************************************************************************
 * INCLUDES
 */

#include "vos_types.h"
#include "trdp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************************************************************
 * DEFINES
 */

#ifndef __cdecl
#define __cdecl
#endif


/***********************************************************************************************************************
 * TYPEDEFS
 */

/** Types for train configuration information */

/** ETB information */
typedef struct
{
    UINT8                   etbId;          /**< identification of train backbone; value range: 0..3 */
    UINT8                   cnCnt;          /**< number of CNs within consist connected to this ETB
                                                 value range 1..16 referring to cnId 0..15 acc. IEC61375-2-5*/
    UINT16                  reserved01;     /**< reserved for future use (= 0) */
} TRDP_ETB_INFO_T;

/** Closed train consists information */
typedef struct
{
    UINT8                   cltrCstUUID[16];    /**< closed train consist UUID */
    UINT8                   cltrCstOrient;      /**< closed train consist orientation
                                                     �01`B = same as closed train direction
                                                     �10`B = inverse to closed train direction */
    UINT8                   cltrCstNo;          /**< sequence number of the consist within the
                                                     closed train, value range 1..32 */
    UINT16                  reserved01;         /**< reserved for future use (= 0) */
} TRDP_CLTRCST_INFO_T;


/** Application defined properties */
typedef struct
{
    TRDP_SHORT_VERSION_T    ver;            /**< properties version information, application defined */
    UINT16                  len;            /**< properties length in number of octets,
                                                 application defined, must be a multiple
                                                 of 4 octets for alignment reasons 
                                                 value range: 0..32768  */
    UINT8                  *pProp;          /**< properties, application defined */
} TRDP_PROP_T;


/** function/device information structure */
typedef struct
{
    TRDP_LABEL_T            fctName;        /**< function device or group label */
    UINT16                  fctId;          /**< unique host identification of the function
                                                 device or group in the consist as defined in
                                                 IEC 61375-2-5, application defined. Value range: 1..16383 */
    BOOL8                   grp;            /**< is a function group and will be resolved as IP multicast address */ 
    UINT8                   reserved01;     /**< reserved for future use (= 0) */
    UINT8                   cstVehNo;       /**< Sequence number of the vehicle in the
                                                 consist the function belongs to. Value range: 1..16, 0 = not defined  */
    UINT8                   etbId;          /**< number of connected train backbone. Value range: 0..3 */
    UINT8                   cnId;           /**< identifier of connected consist network in the consist, 
                                                 related to the etbId. Value range: 0..15 */
    UINT8                   reserved02;     /**< reserved for future use (= 0) */
    TRDP_PROP_T             fctProp;        /**< properties, application defined */
} TRDP_FUNCTION_INFO_T;


/** vehicle information structure */
typedef struct
{
    TRDP_LABEL_T            vehId;          /**< vehicle identifier label,application defined
                                                 (e.g. UIC vehicle identification number) */
    TRDP_LABEL_T            vehType;        /**< vehicle type,application defined */
    UINT8                   vehOrient;      /**< vehicle orientation
                                                 �01`B = same as consist direction
                                                 �10`B = inverse to consist direction */
    UINT8                   vehNo;          /**< Sequence number of vehicle in consist(1..16) */
    ANTIVALENT8             tracVeh;        /**< vehicle is a traction vehicle
                                                 �01`B = vehicle is not a traction vehicle
                                                 �10`B = vehicle is a traction vehicle */
    UINT8                   reserved01;     /**< for future use (= 0) */
    TRDP_PROP_T             vehProp;        /**< static vehicle properties */ 
} TRDP_VEHICLE_INFO_T;


/** consist information structure */
typedef struct
{
    UINT32                  totalLength;    /**< total length of data structure in number of octets */
    TRDP_SHORT_VERSION_T    version;        /**< ConsistInfo data structure version, application defined */
    UINT8                   cstClass;       /**< consist info classification
                                                 0 = (single) consist
                                                 1 = closed train
                                                 2 = closed train consist */
    UINT8                   reserved01;     /**< reserved for future use (= 0) */
    TRDP_LABEL_T            cstId;          /**< consist identifier label, application defined
                                                 (e.g. UIC vehicle identification number
                                                 of the vehicle at extremity 1 of the consist) */
    TRDP_LABEL_T            cstType;        /**< consist type, application defined */
    TRDP_LABEL_T            cstOwner;       /**< consist owner, e.g. "trenitalia.it", "sncf.fr", "db.de" */
    TRDP_UUID_T             cstUuid;        /**< consist UUID  */
    UINT32                  reserved02;     /**< reserved for future use (= 0) */
    TRDP_PROP_T             cstProp;        /**< static consist properties */
    UINT16                  reserved03;     /**< reserved for future use (= 0) */
    UINT16                  etbCnt;         /**< number of ETB�s, range: 1..4 */
    TRDP_ETB_INFO_T        *pEtbInfoList;   /**< ETB information list for the consist
                                                 Ordered list starting with lowest etbId */
    UINT16                  reserved04;     /**< reserved for future use (= 0) */
    UINT16                  vehCnt;         /**< number of vehicles in consist 1..32 */
    TRDP_VEHICLE_INFO_T    *pVehInfoList;   /**< vehicle info list for the vehicles in the consist 
                                                 Ordered list starting with cstVehNo==1 */
    UINT16                  reserved05;     /**< reserved for future use (= 0) */
    UINT16                  fctCnt;         /**< number of consist functions
                                                 value range 0..1024 */
    TRDP_FUNCTION_INFO_T   *pFctInfoList;   /**< function info list for the functions in consist
                                                 lexicographical ordered by fctName */
    UINT16                  reserved06;     /**< reserved for future use (= 0) */
    UINT16                  cltrCstCnt;     /**< number of original consists in closed train 
                                                 value range: 0..32, 0 = consist is no closed train */
    TRDP_CLTRCST_INFO_T    *pCltrCstInfoList; /**< info on closed train composition
                                                 Ordered list starting with cltrCstNo == 1 */
    UINT32                 cstTopoCnt;      /**< sc-32 computed over record, seed value: 'FFFFFFFF'H */
} TRDP_CONSIST_INFO_T;


/* Consist info list */
typedef struct
{
    TRDP_SHORT_VERSION_T    version;        /**< ConsistInfoList structure version  
                                                  parameter �mainVersion� shall be set to 1. */
    UINT16                  cstInfoCnt;     /**< number of consists in train; range: 1..63 */
    TRDP_CONSIST_INFO_T    *pCstInfoList;   /**< consist info collection cstCnt elements */
} TRDP_CONSIST_INFO_LIST_T;


/** TCN consist structure */
typedef struct
{
    TRDP_UUID_T             cstUuid;        /**< Reference to static consist attributes,
                                                 0 if not available (e.g. correction) */
    UINT8                   trnCstNo;       /**< Sequence number of consist in train (1..63) */
    UINT8                   cstOrient;      /**< consist orientation
                                                 �01`B = same as train direction
                                                 �10`B = inverse to train direction */
    UINT8                   reserved01;     /**< reserved for future use (= 0) */
} TRDP_CONSIST_T;


/** TCN train directory */
typedef struct
{
    TRDP_SHORT_VERSION_T    version;        /**< TrainDirectory data structure version  
                                                  parameter �mainVersion� shall be set to 1. */
    BITSET8                 etbId;          /**< identification of the ETB the TTDB is computed for
                                                 bit0: ETB0 (operational network)
                                                 bit1: ETB1 (multimedia network)
                                                 bit2: ETB2 (other network)
                                                 bit3: ETB3 (other network) */
    UINT8                   reserved01;     /**< reserved for future use (= 0) */
    UINT16                  cstCnt;         /**< number of consists in train; range: 1..63 */
    TRDP_CONSIST_T         *pCstDirList;    /**< dynamic consist list ordered list starting with trnCstNo = 1 */
    UINT32                  trnTopoCnt;     /**< sc-32 computed over record (seed value: etbTopoCnt) */
} TRDP_TRAIN_DIRECTORY_T;


/** UIC train directory state */
typedef struct
{
    TRDP_SHORT_VERSION_T    version;        /**< TrainDirectoryState data structure version  
                                                  parameter �mainVersion� shall be set to 1. */
    UINT16                  reserved01;     /**< reserved for future use (= 0) */
    BITSET8                 etbId;          /**< identification of the ETB the TTDB is computed for
                                                 bit0: ETB0 (operational network)
                                                 bit1: ETB1 (multimedia network)
                                                 bit2: ETB2 (other network)
                                                 bit3: ETB3 (other network) */
    UINT8                   trnDirState;    /**< TTDB status: �01`B == unconfirmed, �10`B == confirmed */
    UINT8                   opTrnDirState;  /**< TTDB status: �01`B == inalid, �10`B == valid */
    UINT8                   reserved02;     /**< reserved for future use (= 0) */
    TRDP_LABEL_T            trnId;          /**< train identifier, application defined
                                                 (e.g. �ICE75�, �IC346�), informal */
    TRDP_LABEL_T            trnOperator;    /**< train operator, e.g. �trenitalia.it�, informal */
    UINT32                  opTrnTopoCnt;   /**< operational train topology counter
                                                 set to 0 if opTrnDirState == invalid */
    UINT32                  crc;            /**< sc-32 computed over record (seed value: �FFFFFFFF�H) */
} TRDP_OP_TRAIN_DIRECTORY_STATE_T;


/** UIC operational vehicle structure */
typedef struct
{
    TRDP_LABEL_T            vehId;          /**< Unique vehicle identifier, application defined (e.g. UIC Identifier) */
    UINT8                   opVehNo;        /**< operational vehicle sequence number in train
                                                 value range 1..63 */
    ANTIVALENT8             isLead;         /**< vehicle is leading */
    UINT8                   leadDir;        /**< �01`B = leading direction 1, �10`B = leading direction 2 */
    UINT8                   trnVehNo;       /**< vehicle sequence number within the train
                                                 with vehicle 01 being the first vehicle in ETB reference
                                                 direction 1 as defined in IEC61375-2-5,
                                                 value range: 1..63, a value of 0 indicates that this vehicle has 
                                                 been inserted by correction */
    UINT8                   vehOrient;      /**< vehicle orientation, 
                                                 �01`B = same as operational train direction
                                                 �10`B = inverse to operational train direction */
    UINT8                   ownCstNo;       /**< operational consist number the vehicle belongs to */
    UINT16                  reserved01;     /**< reserved for future use (= 0) */
} TRDP_OP_VEHICLE_T;


/** UIC operational consist structure */
typedef struct
{
    TRDP_UUID_T             cstUuid;        /**< Reference to static consist attributes, 
                                                 0 if not available (e.g. correction) */
    UINT8                   cstIndex;       /**< Index of consist in consist info list, only for performance reason
                                                 in any case cstUUID needs to be checked in parallel */
    UINT8                   opCstNo;        /**< operational consist number in train (1..63) */
    UINT8                   opCstOrient;    /**< consist orientation
                                                 �01`B = same as operational train direction
                                                 �10`B = inverse to operational train direction */
    UINT8                   reserved01;     /*< reserved for future use (= 0) */
} TRDP_OP_CONSIST_T;


/** UIC operational train structure */
typedef struct
{
    TRDP_SHORT_VERSION_T    version;        /**< Train info structure version */
    BITSET8                 etbId;          /**< identification of the ETB the TTDB is computed for
                                                 bit0: ETB0 (operational network)
                                                 bit1: ETB1 (multimedia network)
                                                 bit2: ETB2 (other network)
                                                 bit3: ETB3 (other network) */
    UINT8                   opTrnOrient;    /**< operational train orientation
                                                 �00�B = unknown
                                                 �01`B = same as train direction
                                                 �10`B = inverse to train direction */
    UINT16                  reserved01;     /**< reserved for future use (= 0) */
    UINT16                  opCstCnt;       /**< number of consists in train (1..63) */
    TRDP_OP_CONSIST_T      *pOpCstList;     /**< Pointer to operational consist list starting with op. consist #1 */
    UINT16                  reserved02;     /**< reserved for future use (= 0) */
    UINT16                  opVehCnt;       /**< number of vehicles in train (1..63) */
    TRDP_OP_VEHICLE_T      *pOpVehList;     /**< Pointer to operational vehicle list starting with op. vehicle #1 */
    UINT32                  opTrnTopoCnt;   /**< operational train topology counter 
                                                 SC-32 computed over record (seed value : trnTopoCnt) */
} TRDP_OP_TRAIN_DIRECTORY_T;


/** Train network directory entry structure acc. to IEC61375-2-5 */
typedef struct
{
    UINT8                   cstUUID[16];    /**< unique consist identifier */
    UINT32                  cstNetProp;     /**< consist network properties
                                                 bit0..1:   consist orientation
                                                 bit2..7:   0
                                                 bit8..13:  ETBN Id
                                                 bit14..15: 0
                                                 bit16..21: subnet Id
                                                 bit24..29: CN Id
                                                 bit30..13: 0 */    
} TRDP_TRAIN_NET_DIR_ENTRY_T;


/** Train network directory structure */
typedef struct
{
    UINT16                  reserved01;     /**< reserved for future use (= 0) */
    UINT16                  entryCnt;       /**< number of entries in train network directory */
    TRDP_TRAIN_NET_DIR_ENTRY_T *trnNetDir;  /**< unique consist identifier */
    UINT32                  etbTopoCnt;     /**< train network directory CRC */
} TRDP_TRAIN_NET_DIR_T;


#ifdef __cplusplus
}
#endif

#endif /* TAU_TTI_TYPES_H */
