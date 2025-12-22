/** @file

  Copyright 2006-2016 Unified EFI, Inc.<BR>
  Copyright (c) 2025, Arm Ltd, All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
/*++

Module Name:
  CapsuleOnDiskVariableTestFunction.c

Abstract:
  Source file for Capsule Update "On-Disk" Black-Box Test - Function Test.

  Black-box test for UEFI Required Variables related to capsule update "on disk".
  Verifies CapsuleNNNN, CapsuleMax, and CapsuleLast variables are present and valid
  after a system restart following UpdateCapsule().

Reference:
    UEFI Specification §8.5.6 (Capsule Variable Reporting)

--*/

#include "SctLib.h"
#include "CapsuleOnDiskVariableTestMain.h"
#include <Library/UefiRuntimeServicesTableLib.h>

#define MAX_CAPSULE_VARS  0x0010
#define CAPSULE_MAX_VAR   L"CapsuleMax"
#define CAPSULE_LAST_VAR  L"CapsuleLast"

//
// Prototypes (external)
//

EFI_STATUS
CapsuleOnDiskVariableTest (
  IN EFI_BB_TEST_PROTOCOL       *This,
  IN VOID                       *ClientInterface,
  IN EFI_TEST_LEVEL             TestLevel,
  IN EFI_HANDLE                 SupportHandle
  );

//
// Prototypes (internal)
//

EFI_STATUS
CapsuleOnDiskVariableTestSub1 (
  IN EFI_STANDARD_TEST_LIBRARY_PROTOCOL   *StandardLib,
  IN EFI_TEST_LOGGING_LIBRARY_PROTOCOL    *LoggingLib
  );

 /* CapsuleOnDiskVariableTest - Entry point for Capsule Update On-Disk Function Test.
 *  @param This             A pointer to the EFI_BB_TEST_PROTOCOL instance.
 *  @param ClientInterface  A pointer to the interface to be tested.
 *  @param TestLevel        Test "thoroughness" control.
 *  @param SupportHandle    A handle containing support protocols.
 *  @return EFI_SUCCESS     Successfully.
 *  @return Other value     Something failed.
 */
EFI_STATUS
CapsuleOnDiskVariableTest (
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
                  L"CapsuleUpdateOnDiskTest_func - Failed to test Capsule Update On-Disk",
                  __FILE__,
                  (UINTN)__LINE__
                  )) {
    return EFI_SUCCESS;
  }

  Status = CapsuleOnDiskVariableTestSub1 (StandardLib, LoggingLib);

  return Status;
}

/* ParseCapsuleIndexFromName - Parse capsule index from a CapsuleNNNN variable name.
 *
 * This function parses the numeric capsule index from a Unicode string of the
 * form "Capsule%04X" (for example, "Capsule000F" or "Capsule0001"). The parsed
 * hexadecimal value is returned as a UINTN.
 *
 * @param Name        Pointer to a NULL-terminated Unicode string containing
 *                    the capsule variable name.
 * @param Index       Pointer to a UINTN that receives the parsed capsule index.
 *
 * @return EFI_SUCCESS            Capsule index was successfully parsed.
 * @return EFI_INVALID_PARAMETER  Name or Index is NULL.
 * @return EFI_COMPROMISED_DATA   Name does not follow the expected "Capsule%04X"
 *                                format or contains invalid hexadecimal digits.
 */
STATIC
EFI_STATUS
ParseCapsuleIndexFromName (
  IN  CHAR16 *Name,
  OUT UINTN  *Index
  )
{
  CHAR16 Char;
  UINTN Val = 0;
  UINTN Index1 = 0;

  if (Name == NULL || Index == NULL)
    return EFI_INVALID_PARAMETER;
  if (StrnCmp(Name, L"Capsule", 7) != 0)
    return EFI_COMPROMISED_DATA;

  // Expect 4 hex digits at positions 7..10
  for (Index1 = 7; Index1 < 11; Index1++) {
    Char = Name[Index1];
    Val <<= 4;
    if (Char >= L'0' && Char <= L'9')
      Val |= (Char - L'0');
    else if
      (Char >= L'A' && Char <= L'F') Val |= (Char - L'A' + 10);
    else if
      (Char >= L'a' && Char <= L'f') Val |= (Char - L'a' + 10);
    else
      return EFI_COMPROMISED_DATA;
  }

  *Index = Val;
  return EFI_SUCCESS;
}

 /* CapsuleOnDiskVariableTestSub1 - Entry point for CapsuleUpdateOndisk Sub Function Test.
 *  @param StandardLib    A pointer to the Standard library.
 *  @param LoggingLib     A pointer to the Logging library.
 *  @return EFI_SUCCESS     Successfully.
 *  @return Other value     Something failed.
 */
EFI_STATUS
CapsuleOnDiskVariableTestSub1 (
  IN EFI_STANDARD_TEST_LIBRARY_PROTOCOL   *StandardLib,
  IN EFI_TEST_LOGGING_LIBRARY_PROTOCOL    *LoggingLib
  )
{
  EFI_STATUS        Status;
  UINTN             DataSize;
  UINT32            Attributes;
  UINT8             Buffer[64];
  CHAR16            CapsuleVarName[32];
  UINTN             CapsuleMax;
  UINTN             CapsuleLast;
  UINTN             index;

  //
  // Trace ...
  //
  if (LoggingLib != NULL) {
    LoggingLib->EnterFunction (
                  LoggingLib,
                  L"CapsuleUpdateOnDiskTestSub1",
                  L"EBBR R/2.2.3"
                  );
  }

  //
  // Step 1: Verify CapsuleMax exists
  //
  SctSetMem(Buffer, sizeof(Buffer), 0);
  DataSize = sizeof(Buffer);
  Status = gRT->GetVariable (
                 CAPSULE_MAX_VAR,
                 &gEfiCapsuleReportGuid,
                 &Attributes,
                 &DataSize,
                 Buffer
               );

  if (EFI_ERROR(Status)) {
    StandardLib->RecordAssertion (
      StandardLib,
      EFI_TEST_ASSERTION_WARNING,
      gCapsuleOnDiskVariableTestAssertionGuid001,
      L"CapsuleMax Variable Test: ",
      L"%a:%d: CapsuleMax variable read failed, Status - %r",
       __FILE__, (UINTN)__LINE__, Status
    );
    CapsuleMax = MAX_CAPSULE_VARS;
  } else {
    Status = ParseCapsuleIndexFromName((CHAR16 *)Buffer, &CapsuleMax);
    if (EFI_ERROR(Status)) {
      SctPrint(L"Failed to extract CapsuleMax (Maximum Capsule Update) Count Value\n");
      CapsuleMax = MAX_CAPSULE_VARS;
    };
    CapsuleMax += 1;
    SctPrint(L"CapsuleMax (Maximum Capsule Update) Count Value: %d\n", CapsuleMax);

    StandardLib->RecordAssertion (
      StandardLib,
      EFI_TEST_ASSERTION_PASSED,
      gCapsuleOnDiskVariableTestAssertionGuid001,
      L"CapsuleMax Variable Test: ",
      L"%a:%d: %s is found, CapsuleMax Value - %d, Status - %r",
      __FILE__, (UINTN)__LINE__, Buffer, CapsuleMax, Status
    );
  }

  //
  // Step 2: Verify CapsuleLast exists
  //
  SctSetMem(Buffer, sizeof(Buffer), 0);
  DataSize = sizeof(Buffer);
  Status = gRT->GetVariable (
                 CAPSULE_LAST_VAR,
                 &gEfiCapsuleReportGuid,
                 &Attributes,
                 &DataSize,
                 Buffer
               );

  if (EFI_ERROR(Status)) {
    StandardLib->RecordAssertion (
      StandardLib,
      EFI_TEST_ASSERTION_FAILED,
      gCapsuleOnDiskVariableTestAssertionGuid002,
      L"CapsuleLast Variable Test: ",
      L"%a:%d: CapsuleLast variable read failed, Status - %r",
       __FILE__, (UINTN)__LINE__, Status
    );
  } else {
    Status = ParseCapsuleIndexFromName((CHAR16 *)Buffer, &CapsuleLast);
    if (EFI_ERROR(Status)) {
      SctPrint(L"Failed to extract CapsuleLast (Last Capsule Update) Index Value\n");
    };
    SctPrint(L"CapsuleLast(Last Capsule Update) Index Value: %d\n", CapsuleLast);

    StandardLib->RecordAssertion (
      StandardLib,
      EFI_TEST_ASSERTION_PASSED,
      gCapsuleOnDiskVariableTestAssertionGuid002,
      L"CapsuleLast Variable Test: : ",
      L"%a:%d: %s is found, CapsuleLast Value - %d, Status - %r",
      __FILE__, (UINTN)__LINE__, Buffer, CapsuleLast, Status
    );
  }

  //
  // Step 3: Verify CapsuleNNNN variables up to CapsuleMax
  //
  SctSetMem(Buffer, sizeof(Buffer), 0);
  SctSetMem(CapsuleVarName, sizeof(CapsuleVarName), 0);
  for (index = 0; index < CapsuleMax && index < MAX_CAPSULE_VARS; index++) {
    UnicodeSPrint (CapsuleVarName, sizeof(CapsuleVarName), L"Capsule%04x", index);

    DataSize = sizeof(Buffer);
    Status = gRT->GetVariable (
                   CapsuleVarName,
                   &gEfiCapsuleReportGuid,
                   &Attributes,
                   &DataSize,
                   Buffer
                 );

    if (Status == EFI_NOT_FOUND) {
      StandardLib->RecordAssertion (
        StandardLib,
        EFI_TEST_ASSERTION_WARNING,
        gCapsuleOnDiskVariableTestAssertionGuid003,
        L"CapsuleNNNN Varaible Test: ",
        L"%a:%d: %s variable is not found, Status=%r",
         __FILE__, (UINTN)__LINE__, CapsuleVarName, Status
      );
    } else if (EFI_ERROR(Status)) {
      StandardLib->RecordAssertion (
        StandardLib,
        EFI_TEST_ASSERTION_FAILED,
        gCapsuleOnDiskVariableTestAssertionGuid003,
        L"CapsuleNNNN Varaible Test: ",
        L"%a:%d: %s variable read failed, Status=%r",
         __FILE__, (UINTN)__LINE__, CapsuleVarName, Status
      );
    } else {
      StandardLib->RecordAssertion (
        StandardLib,
        EFI_TEST_ASSERTION_PASSED,
        gCapsuleOnDiskVariableTestAssertionGuid003,
        L"CapsuleNNNN Variable Test:  ",
        L"%a:%d: %s variable is found, Status=%r",
        __FILE__, (UINTN)__LINE__, CapsuleVarName, Status
      );
    }
  }

  //
  // Trace ...
  //
  if (LoggingLib != NULL) {
    LoggingLib->ExitFunction (
                  LoggingLib,
                  L"CapsuleUpdateOnDiskTestSub1",
                  L"EBBR R/2.2.3"
                  );
  }

  //
  // Done
  //
  return EFI_SUCCESS;
}
