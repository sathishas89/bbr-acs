/** @file

  Copyright (c) 2026, Arm Ltd. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
/*++

Module Name:
  SecureBootAuthVariablePersistenceBBTestMain.h

Abstract:
  Header file for Secure Boot variable persistence black-box test.

--*/

#ifndef SECURE_BOOT_AUTH_VARIABLE_PERSISTENCE_BB_TEST_MAIN_H
#define SECURE_BOOT_AUTH_VARIABLE_PERSISTENCE_BB_TEST_MAIN_H

#include "Efi.h"
#include "Guid.h"
#include <Library/EfiTestLib.h>

#include EFI_TEST_PROTOCOL_DEFINITION(TestRecoveryLibrary)
#include EFI_TEST_PROTOCOL_DEFINITION(TestLoggingLibrary)

#define SECURE_BOOT_AUTH_VARIABLE_PERSISTENCE_BB_TEST_REVISION  0x00010000

#define SECURE_BOOT_AUTH_VARIABLE_PERSISTENCE_BB_TEST_GUID \
  { 0x9adfc05f, 0xcebe, 0x496a, { 0x9c, 0x8a, 0xd7, 0x46, 0xdd, 0x0d, 0xbf, 0xd0 } }

#define SECURE_BOOT_AUTH_VARIABLE_GUID \
  { 0x6f5345c5, 0x4f42, 0x4e8e, { 0x9a, 0x5c, 0x87, 0xbf, 0x48, 0xa6, 0xd7, 0x2c } }

#define SECURE_BOOT_AUTH_VARIABLE_NAME  L"SecureBootAuthPersistenceVar"
#define SECURE_BOOT_AUTH_VARIABLE_PAYLOAD \
  "Secure Boot authenticated variable persistence test"
#define SECURE_BOOT_AUTH_VARIABLE_ATTRIBUTES \
  (EFI_VARIABLE_NON_VOLATILE | \
   EFI_VARIABLE_BOOTSERVICE_ACCESS | \
   EFI_VARIABLE_RUNTIME_ACCESS | \
   EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS)

EFI_STATUS
SecureBootAuthVariablePersistenceTest (
  IN EFI_BB_TEST_PROTOCOL  *This,
  IN VOID                  *ClientInterface,
  IN EFI_TEST_LEVEL        TestLevel,
  IN EFI_HANDLE            SupportHandle
  );

EFI_STATUS
GetTestSupportLibrary (
  IN EFI_HANDLE                           SupportHandle,
  OUT EFI_STANDARD_TEST_LIBRARY_PROTOCOL  **StandardLib,
  OUT EFI_TEST_RECOVERY_LIBRARY_PROTOCOL  **RecoveryLib,
  OUT EFI_TEST_LOGGING_LIBRARY_PROTOCOL   **LoggingLib
  );

#endif
