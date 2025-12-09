/** @file

  Copyright 2006-2016 Unified EFI, Inc.<BR>
  Copyright (c) 2025, Arm Ltd. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at 
  http://opensource.org/licenses/bsd-license.php
 
  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
 
**/
/*++

Module Name:
 EfiConformanceProfileTableTestFunction.c

Abstract:
  Source file for EFI Conformance Profile Table Black-Box Test - Function Test.

--*/

#include "SctLib.h"
#include "EfiConformanceProfileTableTestMain.h"

//
// Prototypes (external)
//

EFI_STATUS
EfiConformanceProfileTableTest (
  IN EFI_BB_TEST_PROTOCOL       *This,
  IN VOID                       *ClientInterface,
  IN EFI_TEST_LEVEL             TestLevel,
  IN EFI_HANDLE                 SupportHandle
  );

//
// Prototypes (internal)
//

EFI_STATUS
EfiConformanceProfileTableTestSub1 (
  IN EFI_STANDARD_TEST_LIBRARY_PROTOCOL   *StandardLib,
  IN EFI_TEST_LOGGING_LIBRARY_PROTOCOL    *LoggingLib
  );

 /* EfiConformanceProfileTableTest - Entry point for EFI Conformance Profile Table Function Test.
 *  @param This             A pointer to the EFI_BB_TEST_PROTOCOL instance.
 *  @param ClientInterface  A pointer to the interface to be tested.
 *  @param TestLevel        Test "thoroughness" control.
 *  @param SupportHandle    A handle containing support protocols.
 *  @return EFI_SUCCESS     Successfully.
 *  @return Other value     Something failed.
 */
EFI_STATUS
EfiConformanceProfileTableTest (
  IN EFI_BB_TEST_PROTOCOL       *This,
  IN VOID                       *ClientInterface,
  IN EFI_TEST_LEVEL             TestLevel,
  IN EFI_HANDLE                 SupportHandle
  )
{
  EFI_STATUS                          Status;
  EFI_STANDARD_TEST_LIBRARY_PROTOCOL  *StandardLib;
  EFI_TEST_RECOVERY_LIBRARY_PROTOCOL  *RecoveryLib;
  EFI_TEST_LOGGING_LIBRARY_PROTOCOL   *LoggingLib;

  //
  // Get test support library interfaces
  //
  Status = GetTestSupportLibrary (
             SupportHandle,
             &StandardLib,
             &RecoveryLib,
             &LoggingLib
             );
  if (EFI_ERROR(Status)) {
    return Status;
  }

  if (FALSE == CheckBBTestCanRunAndRecordAssertion(
                  StandardLib, 
                  L"EBBR EbbrProfileTableTest_func - Failed to test EBBR profile table",
                  __FILE__,
                  (UINTN)__LINE__
                  )) {
    return EFI_SUCCESS;
  }

  Status = EfiConformanceProfileTableTestSub1 (StandardLib, LoggingLib);

  return Status;
}

 /* EfiConformanceProfileTableTestSub1 - Entry point for EFI Conformance Profile Table Sub Function Test.
 *  @param StandardLib    A pointer to the Standard library.
 *  @param LoggingLib     A pointer to the Logging library.
 *  @return EFI_SUCCESS     Successfully.
 *  @return Other value     Something failed.
 */
EFI_STATUS
EfiConformanceProfileTableTestSub1 (
  IN EFI_STANDARD_TEST_LIBRARY_PROTOCOL   *StandardLib,
  IN EFI_TEST_LOGGING_LIBRARY_PROTOCOL    *LoggingLib
  )
{
  EFI_STATUS                       Status = EFI_SUCCESS;
  EFI_STATUS                       FindStatus = EFI_NOT_FOUND;
  EFI_CONFORMANCE_PROFILES_TABLE  *ConfProfTable = NULL;
  EFI_GUID                        *EntryGuid = NULL;
  CONST CHAR16                    *EbbrProfileName = L"Unknown";
  UINTN                            Profile = 0;
  BOOLEAN                          EbbrFound = FALSE;

  //
  // Trace ...
  //
  if (LoggingLib != NULL) {
    LoggingLib->EnterFunction (
                  LoggingLib,
                  L"EfiConformanceProfileTableTest",
                  L"EBBR R/2.4.1"
                  );
  }

  Status = SctGetSystemConfigurationTable (&gEfiConfProfilesTableGuid, (VOID **)&ConfProfTable);
  if (!EFI_ERROR (Status) && (ConfProfTable != NULL)) {

    EntryGuid = (EFI_GUID *)(ConfProfTable + 1);

    for (Profile = 0; Profile < (UINTN)ConfProfTable->NumberOfProfiles; Profile++, EntryGuid++) {
      if (CompareGuid (EntryGuid, &gEfiConfProfilesEbbrSpec22Guid)) {
        EbbrProfileName = L"EBBR_2.2";
        EbbrFound = TRUE;
        break;
      } else if (CompareGuid (EntryGuid, &gEfiConfProfilesEbbrSpec21Guid)) {
        EbbrProfileName = L"EBBR_2.1";
        EbbrFound = TRUE;
        break;
      }

    }

    FindStatus = EbbrFound ? EFI_SUCCESS : EFI_NOT_FOUND;

    if (!EbbrFound) {
      StandardLib->RecordAssertion (
        StandardLib,
        EFI_TEST_ASSERTION_WARNING,
        gEfiConformanceProfileTableTestAssertionGuid001,
        L"EFI Conformance Profile Table test",
        L"EBBR profile not found in EFI Conformance Profiles Table "
        L"(NumberOfProfiles=%d). %a:%d Status - %r",
        (INT32)ConfProfTable->NumberOfProfiles,
        __FILE__,
        (UINTN)__LINE__,
        FindStatus
        );
    } else {
      StandardLib->RecordAssertion (
        StandardLib,
        EFI_TEST_ASSERTION_PASSED,
        gEfiConformanceProfileTableTestAssertionGuid001,
        L"EFI Conformance Profile Table test",
        L"Found %s profile in EFI Conformance Profiles Table. %a:%d Status - %r",
        EbbrProfileName,
        __FILE__,
        (UINTN)__LINE__,
        FindStatus
        );
    }
  } else {
    StandardLib->RecordAssertion (
      StandardLib,
      EFI_TEST_ASSERTION_WARNING,
      gEfiConformanceProfileTableTestAssertionGuid002,
      L"EFI Conformance Profiles Table not found",
      L"%a:%d Status - %r",
      __FILE__,
      (UINTN)__LINE__,
      Status
      );
  }

  //
  // Trace ...
  //
  if (LoggingLib != NULL) {
    LoggingLib->ExitFunction (
                  LoggingLib,
                  L"EfiConformanceProfileTableTest",
                  L"EBBR R/2.4.1"
                  );
  }

  //
  // Done
  //
  return EFI_SUCCESS;
}
