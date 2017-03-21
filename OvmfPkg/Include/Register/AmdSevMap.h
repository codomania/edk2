/** @file

AMD Secure Encrypted Virtualization (SEV) specific CPUID and MSR definitions

The complete description for this CPUID leaf is available in APM volume 2 (Section 15.34)
http://support.amd.com/TechDocs/24593.pdf

Copyright (c) 2017, Advanced Micro Devices. All rights reserved.<BR>

This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __AMD_SEV_MAP_H__
#define __AMD_SEV_MAP_H__

#pragma pack (1)

#define CPUID_MEMORY_ENCRYPTION_INFO             0x8000001F

/**
  CPUID Memory Encryption support information EAX for CPUID leaf
  #CPUID_MEMORY_ENCRYPTION_INFO.
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Secure Memory Encryption (Sme) Support
    ///
    UINT32  SmeBit:1;

    ///
    /// [Bit 1] Secure Encrypted Virtualization (Sev) Support
    ///
    UINT32  SevBit:1;

    ///
    /// [Bit 2] Page flush MSR support
    ///
    UINT32  PageFlushMsrBit:1;

    ///
    /// [Bit 3] Encrypted state support
    ///
    UINT32  SevEsBit:1;

    ///
    /// [Bit 4:31] Reserved
    ///
    UINT32  ReservedBits:28;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
} CPUID_MEMORY_ENCRYPTION_INFO_EAX;

/**
  CPUID Memory Encryption support information EBX for CPUID leaf
  #CPUID_MEMORY_ENCRYPTION_INFO.
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0:5] Page table bit number used to enable memory encryption
    ///
    UINT32  PtePosBits:6;

    ///
    /// [Bit 6:11] Reduction of system physical address space bits when memory encryption is enabled
    ///
    UINT32  ReducedPhysBits:5;

    ///
    /// [Bit 12:31] Reserved
    ///
    UINT32  ReservedBits:21;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
} CPUID_MEMORY_ENCRYPTION_INFO_EBX;

/**
  Secure Encrypted Virtualization (SEV) status register

**/
#define MSR_SEV_STATUS                     0xc0010131

/**
  MSR information returned for #MSR_SEV_STATUS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Secure Encrypted Virtualization (Sev) is enabled
    ///
    UINT32  SevBit:1;

    ///
    /// [Bit 1] Secure Encrypted Virtualization Encrypted State (SevEs) is enabled
    ///
    UINT32  SevEsBit:1;

    UINT32  Reserved:30;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SEV_STATUS_REGISTER;

#endif
