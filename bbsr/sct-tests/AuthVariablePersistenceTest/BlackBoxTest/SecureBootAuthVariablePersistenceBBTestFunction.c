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
  SecureBootAuthVariablePersistenceBBTestFunction.c

Abstract:
  Source file for Secure Boot authenticated variable persistence black-box test.

--*/

#include "SctLib.h"
#include "SecureBootAuthVariablePersistenceBBTestMain.h"
#include "SecureBootAuthVariablePersistenceAuthData.inc"

STATIC CONST UINTN gSecureBootAuthVariableCreateDataSize =
  sizeof (gSecureBootAuthVariableCreateData);
STATIC CONST UINTN gSecureBootAuthVariableDeleteDataSize =
  sizeof (gSecureBootAuthVariableDeleteData);

#define SCT_RESET_RECORD_BUFFER_SIZE  1024U
#define VERIFY_AFTER_RESET_MARKER      1U

STATIC EFI_GUID mSecureBootAuthVariableGuid = SECURE_BOOT_AUTH_VARIABLE_GUID;

STATIC CONST UINT8 mExpectedVariablePayload[] =
  SECURE_BOOT_AUTH_VARIABLE_PAYLOAD;

STATIC CONST UINT8 mUnauthenticatedPayload[] =
  "Unauthenticated update must be rejected";

/**
  Reads the test variable and returns the values needed for validation.

  @param RT                A pointer to the EFI Runtime Services table.
  @param VariablePresent   On return, TRUE if the variable exists.
  @param Attributes        On return, the variable attributes.
  @param VariableDataSize  On return, the variable payload size.
  @param VariableData      On return, allocated variable data, or NULL. The
                           caller must free a non-NULL buffer with FreePool().

  @retval EFI_SUCCESS           The variable was read, or is not present.
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failed.
  @return Other value           An error returned by a runtime or boot service.
**/
STATIC
EFI_STATUS
ReadTestVariable (
  IN EFI_RUNTIME_SERVICES  *RT,
  OUT BOOLEAN              *VariablePresent,
  OUT UINT32               *Attributes,
  OUT UINTN                *VariableDataSize,
  OUT VOID                 **VariableData
  )
{
  EFI_STATUS  Status;
  UINTN       DataSize;
  UINTN       Retry;
  VOID        *Data;

  *VariablePresent = FALSE;
  *Attributes = 0;
  *VariableDataSize = 0;
  *VariableData = NULL;

  DataSize = 0;
  Status = RT->GetVariable (
                 SECURE_BOOT_AUTH_VARIABLE_NAME,
                 &mSecureBootAuthVariableGuid,
                 Attributes,
                 &DataSize,
                 NULL
                 );
  if (Status == EFI_NOT_FOUND) {
    return EFI_SUCCESS;
  }

  if ((Status != EFI_SUCCESS) && (Status != EFI_BUFFER_TOO_SMALL)) {
    return Status;
  }

  *VariablePresent = TRUE;
  *VariableDataSize = DataSize;
  if (DataSize == 0) {
    return EFI_SUCCESS;
  }

  Data = NULL;
  for (Retry = 0; Retry < 2; Retry++) {
    if (DataSize == MAX_UINTN) {
      return EFI_BAD_BUFFER_SIZE;
    }

    Status = gtBS->AllocatePool (
                     EfiBootServicesData,
                     DataSize + 1,
                     &Data
                     );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = RT->GetVariable (
                   SECURE_BOOT_AUTH_VARIABLE_NAME,
                   &mSecureBootAuthVariableGuid,
                   Attributes,
                   &DataSize,
                   Data
                   );
    if (Status == EFI_BUFFER_TOO_SMALL) {
      gtBS->FreePool (Data);
      Data = NULL;
      continue;
    }

    if (EFI_ERROR (Status)) {
      gtBS->FreePool (Data);
      return Status;
    }

    ((UINT8 *)Data)[DataSize] = 0;
    *VariableDataSize = DataSize;
    *VariableData = Data;
    return EFI_SUCCESS;
  }

  return EFI_BUFFER_TOO_SMALL;
}

/**
  Logs the authenticated test variable state.

  @param StandardLib      A pointer to the Standard Test Library protocol.
  @param VariablePresent  TRUE if the variable exists.
  @param Attributes       The variable attributes.
  @param VariableDataSize The variable payload size.
  @param VariableData     The variable payload.
**/
STATIC
VOID
LogTestVariableState (
  IN EFI_STANDARD_TEST_LIBRARY_PROTOCOL  *StandardLib,
  IN BOOLEAN                             VariablePresent,
  IN UINT32                              Attributes,
  IN UINTN                               VariableDataSize,
  IN VOID                                *VariableData
  )
{
  if (!VariablePresent) {
    StandardLib->RecordMessage (
                   StandardLib,
                   EFI_VERBOSE_LEVEL_DEFAULT,
                   L"%s is not present.",
                   SECURE_BOOT_AUTH_VARIABLE_NAME
                   );
    return;
  }

  StandardLib->RecordMessage (
                 StandardLib,
                 EFI_VERBOSE_LEVEL_DEFAULT,
                 L"%s present: attr=0x%08x size=%d",
                 SECURE_BOOT_AUTH_VARIABLE_NAME,
                 Attributes,
                 VariableDataSize
                 );

  if ((VariableDataSize > 0) && (VariableData != NULL)) {
    StandardLib->RecordMessage (
                   StandardLib,
                   EFI_VERBOSE_LEVEL_DEFAULT,
                   L"%s data: \"%a\"",
                   SECURE_BOOT_AUTH_VARIABLE_NAME,
                   VariableData
                   );
  }
}

/**
  Validates the test variable attributes and exact payload.

  @param StandardLib      A pointer to the Standard Test Library protocol.
  @param VariablePresent  TRUE if the variable exists.
  @param Attributes       The variable attributes.
  @param VariableDataSize The variable payload size.
  @param VariableData     The variable payload.

  @retval TRUE   The variable has the expected attributes and payload.
  @retval FALSE  One or more values do not match.
**/
STATIC
BOOLEAN
ValidateTestVariable (
  IN EFI_STANDARD_TEST_LIBRARY_PROTOCOL  *StandardLib,
  IN BOOLEAN                             VariablePresent,
  IN UINT32                              Attributes,
  IN UINTN                               VariableDataSize,
  IN VOID                                *VariableData
  )
{
  BOOLEAN  Valid;
  UINTN    ExpectedDataSize;

  Valid = TRUE;
  ExpectedDataSize = sizeof (mExpectedVariablePayload) - 1;

  if (!VariablePresent) {
    StandardLib->RecordMessage (
                   StandardLib,
                   EFI_VERBOSE_LEVEL_DEFAULT,
                   L"Authenticated test variable is missing."
                   );
    return FALSE;
  }

  if (Attributes != SECURE_BOOT_AUTH_VARIABLE_ATTRIBUTES) {
    StandardLib->RecordMessage (
                   StandardLib,
                   EFI_VERBOSE_LEVEL_DEFAULT,
                   L"Unexpected test variable attributes: 0x%08x, expected 0x%08x.",
                   Attributes,
                   SECURE_BOOT_AUTH_VARIABLE_ATTRIBUTES
                   );
    Valid = FALSE;
  }

  if (VariableDataSize != ExpectedDataSize) {
    StandardLib->RecordMessage (
                   StandardLib,
                   EFI_VERBOSE_LEVEL_DEFAULT,
                   L"Unexpected test variable size: %d, expected %d.",
                   VariableDataSize,
                   ExpectedDataSize
                   );
    Valid = FALSE;
  } else if ((VariableData == NULL) ||
             (SctCompareMem (
                VariableData,
                (VOID *)mExpectedVariablePayload,
                ExpectedDataSize
                ) != 0)) {
    StandardLib->RecordMessage (
                   StandardLib,
                   EFI_VERBOSE_LEVEL_DEFAULT,
                   L"Test variable payload does not match the expected value."
                   );
    Valid = FALSE;
  }

  return Valid;
}

/**
  Records the result of the authenticated variable persistence test.

  @param StandardLib    A pointer to the Standard Test Library protocol.
  @param AssertionType  The assertion result to record.
  @param Detail         The assertion detail string.
  @param LineNumber     The source line associated with the assertion.

  @retval EFI_SUCCESS  The assertion was recorded.
  @return Other value  The assertion could not be recorded.
**/
STATIC
EFI_STATUS
RecordPersistenceAssertion (
  IN EFI_STANDARD_TEST_LIBRARY_PROTOCOL  *StandardLib,
  IN EFI_TEST_ASSERTION                  AssertionType,
  IN CHAR16                              *Detail,
  IN UINTN                               LineNumber
  )
{
  return StandardLib->RecordAssertion (
                 StandardLib,
                 AssertionType,
                 gSecureBootAuthVariablePersistenceBbTestAssertionGuid001,
                 L"RT.SecureBootAuthVariablePersistenceTest - authenticated "
                 L"variable persists across reset",
                 L"%a:%d: %s",
                 __FILE__,
                 LineNumber,
                 Detail
                 );
}

/**
  Checks whether the uniquely named persistence test variable exists.

  @param RT      A pointer to the EFI Runtime Services table.
  @param Exists  On return, TRUE if the variable exists.

  @retval EFI_SUCCESS  The existence check completed successfully.
  @return Other value  An error returned by GetVariable().
**/
STATIC
EFI_STATUS
CheckTestVariableExists (
  IN EFI_RUNTIME_SERVICES  *RT,
  OUT BOOLEAN              *Exists
  )
{
  EFI_STATUS  Status;
  UINT32      Attributes;
  UINTN       DataSize;

  *Exists = FALSE;
  Attributes = 0;
  DataSize = 0;

  Status = RT->GetVariable (
                 SECURE_BOOT_AUTH_VARIABLE_NAME,
                 &mSecureBootAuthVariableGuid,
                 &Attributes,
                 &DataSize,
                 NULL
                 );
  if (Status == EFI_NOT_FOUND) {
    return EFI_SUCCESS;
  }

  if ((Status == EFI_SUCCESS) || (Status == EFI_BUFFER_TOO_SMALL)) {
    *Exists = TRUE;
    return EFI_SUCCESS;
  }

  return Status;
}

/**
  Deletes the owned test variable with the supplied authenticated request and
  confirms that it no longer exists.

  @param RT                    A pointer to the EFI Runtime Services table.
  @param StandardLib           A pointer to the Standard Test Library protocol.
  @param AuthenticatedData     The authenticated deletion descriptor.
  @param AuthenticatedDataSize The descriptor size.

  @retval EFI_SUCCESS    The variable is no longer present.
  @retval EFI_NOT_FOUND  The required primary deletion found no variable.
  @return Other value    A runtime error occurred.
**/
STATIC
EFI_STATUS
DeleteOwnedTestVariable (
  IN EFI_RUNTIME_SERVICES                 *RT,
  IN EFI_STANDARD_TEST_LIBRARY_PROTOCOL   *StandardLib,
  IN CONST UINT8                          *AuthenticatedData,
  IN UINTN                                AuthenticatedDataSize
  )
{
  EFI_STATUS  Status;
  BOOLEAN     VariableExists;

  Status = CheckTestVariableExists (RT, &VariableExists);
  if (EFI_ERROR (Status)) {
    StandardLib->RecordMessage (
                   StandardLib,
                   EFI_VERBOSE_LEVEL_DEFAULT,
                   L"Unable to query the test variable before deletion: %r",
                   Status
                   );
    return Status;
  }

  if (!VariableExists) {
    StandardLib->RecordMessage (
                   StandardLib,
                   EFI_VERBOSE_LEVEL_DEFAULT,
                   L"Authenticated test variable was absent before required deletion."
                   );
    return EFI_NOT_FOUND;
  }

  Status = RT->SetVariable (
                 SECURE_BOOT_AUTH_VARIABLE_NAME,
                 &mSecureBootAuthVariableGuid,
                 SECURE_BOOT_AUTH_VARIABLE_ATTRIBUTES,
                 AuthenticatedDataSize,
                 (VOID *)AuthenticatedData
                 );
  if (EFI_ERROR (Status)) {
    StandardLib->RecordMessage (
                   StandardLib,
                   EFI_VERBOSE_LEVEL_DEFAULT,
                   L"Authenticated deletion failed: %r",
                   Status
                   );
    return Status;
  }

  Status = CheckTestVariableExists (RT, &VariableExists);
  if (EFI_ERROR (Status)) {
    StandardLib->RecordMessage (
                   StandardLib,
                   EFI_VERBOSE_LEVEL_DEFAULT,
                   L"Unable to verify authenticated deletion: %r",
                   Status
                   );
    return Status;
  }

  if (VariableExists) {
    StandardLib->RecordMessage (
                   StandardLib,
                   EFI_VERBOSE_LEVEL_DEFAULT,
                   L"Test variable remains after authenticated deletion."
                   );
    return EFI_DEVICE_ERROR;
  }

  StandardLib->RecordMessage (
                 StandardLib,
                 EFI_VERBOSE_LEVEL_DEFAULT,
                 L"Authenticated test variable was deleted successfully."
                 );
  return EFI_SUCCESS;
}

/**
  Reads the test variable and validates its expected attributes and payload.

  @param RT           A pointer to the EFI Runtime Services table.
  @param StandardLib  A pointer to the Standard Test Library protocol.
  @param Valid        On return, TRUE if the variable has the expected value.

  @retval EFI_SUCCESS  The variable was read and validation completed.
  @return Other value  The variable could not be read.
**/
STATIC
EFI_STATUS
ReadAndValidateTestVariable (
  IN EFI_RUNTIME_SERVICES                *RT,
  IN EFI_STANDARD_TEST_LIBRARY_PROTOCOL  *StandardLib,
  OUT BOOLEAN                            *Valid
  )
{
  EFI_STATUS  Status;
  BOOLEAN     VariablePresent;
  UINT32      Attributes;
  UINTN       VariableDataSize;
  VOID        *VariableData;

  VariableData = NULL;
  Status = ReadTestVariable (
             RT,
             &VariablePresent,
             &Attributes,
             &VariableDataSize,
             &VariableData
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  LogTestVariableState (
    StandardLib,
    VariablePresent,
    Attributes,
    VariableDataSize,
    VariableData
    );
  *Valid = ValidateTestVariable (
             StandardLib,
             VariablePresent,
             Attributes,
             VariableDataSize,
             VariableData
             );

  if (VariableData != NULL) {
    gtBS->FreePool (VariableData);
  }

  return EFI_SUCCESS;
}

/**
  Records a pre-reset failure after attempting authenticated cleanup.

  The normal delete descriptor is safe to use here because the test will not
  proceed to reset recovery after recording a failure.

  @param RT           A pointer to the EFI Runtime Services table.
  @param StandardLib  A pointer to the Standard Test Library protocol.
  @param Detail       The assertion detail string.
  @param LineNumber   The source line associated with the assertion.

  @retval EFI_SUCCESS  The failed assertion was recorded.
  @return Other value  The assertion could not be recorded.
**/
STATIC
EFI_STATUS
RecordPreResetFailureAndCleanup (
  IN EFI_RUNTIME_SERVICES                *RT,
  IN EFI_STANDARD_TEST_LIBRARY_PROTOCOL  *StandardLib,
  IN CHAR16                              *Detail,
  IN UINTN                               LineNumber
  )
{
  EFI_STATUS  DeleteStatus;

  DeleteStatus = DeleteOwnedTestVariable (
                   RT,
                   StandardLib,
                   gSecureBootAuthVariableDeleteData,
                   gSecureBootAuthVariableDeleteDataSize
                   );
  if (EFI_ERROR (DeleteStatus)) {
    StandardLib->RecordMessage (
                   StandardLib,
                   EFI_VERBOSE_LEVEL_DEFAULT,
                   L"Failed to clean up the authenticated test variable: %r",
                   DeleteStatus
                   );
  }

  return RecordPersistenceAssertion (
           StandardLib,
           EFI_TEST_ASSERTION_FAILED,
           Detail,
           LineNumber
           );
}

/**
  Creates the unique authenticated variable, prepares SCT recovery, and
  resets the system.

  @param RT           A pointer to the EFI Runtime Services table.
  @param StandardLib  A pointer to the Standard Test Library protocol.
  @param RecoveryLib  A pointer to the Test Recovery Library protocol.

  @retval EFI_SUCCESS  The test result was recorded, or the system reset.
**/
STATIC
EFI_STATUS
CreateAuthenticatedTestVariableAndReset (
  IN EFI_RUNTIME_SERVICES                 *RT,
  IN EFI_STANDARD_TEST_LIBRARY_PROTOCOL   *StandardLib,
  IN EFI_TEST_RECOVERY_LIBRARY_PROTOCOL   *RecoveryLib
  )
{
  EFI_STATUS  Status;
  BOOLEAN     VariableExists;
  BOOLEAN     Valid;
  UINT8       ResetMarker;

  Status = CheckTestVariableExists (RT, &VariableExists);
  if (EFI_ERROR (Status)) {
    StandardLib->RecordMessage (
                   StandardLib,
                   EFI_VERBOSE_LEVEL_DEFAULT,
                   L"Unable to check for the test variable: %r",
                   Status
                   );
    return RecordPersistenceAssertion (
             StandardLib,
             EFI_TEST_ASSERTION_FAILED,
             L"Could not determine whether the test variable already exists.",
             (UINTN)__LINE__
             );
  }

  if (VariableExists) {
    return RecordPersistenceAssertion (
             StandardLib,
             EFI_TEST_ASSERTION_FAILED,
             L"The test-specific authenticated variable already exists before the test.",
             (UINTN)__LINE__
             );
  }

  Status = RT->SetVariable (
                 SECURE_BOOT_AUTH_VARIABLE_NAME,
                 &mSecureBootAuthVariableGuid,
                 SECURE_BOOT_AUTH_VARIABLE_ATTRIBUTES,
                 gSecureBootAuthVariableCreateDataSize,
                 (VOID *)gSecureBootAuthVariableCreateData
                 );
  if (EFI_ERROR (Status)) {
    StandardLib->RecordMessage (
                   StandardLib,
                   EFI_VERBOSE_LEVEL_DEFAULT,
                   L"Authenticated variable creation failed: %r",
                   Status
                   );
    return RecordPersistenceAssertion (
             StandardLib,
             EFI_TEST_ASSERTION_FAILED,
             L"Could not create the test-specific authenticated variable.",
             (UINTN)__LINE__
             );
  }

  Status = ReadAndValidateTestVariable (RT, StandardLib, &Valid);
  if (EFI_ERROR (Status)) {
    StandardLib->RecordMessage (
                   StandardLib,
                   EFI_VERBOSE_LEVEL_DEFAULT,
                   L"Could not read the newly created variable: %r",
                   Status
                   );
    return RecordPreResetFailureAndCleanup (
             RT,
             StandardLib,
             L"Could not validate the newly created authenticated variable.",
             (UINTN)__LINE__
             );
  }

  if (!Valid) {
    return RecordPreResetFailureAndCleanup (
             RT,
             StandardLib,
             L"The newly created authenticated variable is invalid.",
             (UINTN)__LINE__
             );
  }

  ResetMarker = VERIFY_AFTER_RESET_MARKER;
  Status = RecoveryLib->WriteResetRecord (
                          RecoveryLib,
                          sizeof (ResetMarker),
                          &ResetMarker
                          );
  if (EFI_ERROR (Status)) {
    StandardLib->RecordMessage (
                   StandardLib,
                   EFI_VERBOSE_LEVEL_DEFAULT,
                   L"Could not write the reset-recovery record: %r",
                   Status
                   );
    return RecordPreResetFailureAndCleanup (
             RT,
             StandardLib,
             L"Could not save test state before reset.",
             (UINTN)__LINE__
             );
  }

  StandardLib->RecordMessage (
                 StandardLib,
                 EFI_VERBOSE_LEVEL_DEFAULT,
                 L"Authenticated test variable created and validated before reset."
                 );
  SctPrint (
    L"System will cold reset after 1 second to verify authenticated variable persistence.\n"
    );
  gtBS->Stall (1000000);
  RT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);

  StandardLib->RecordMessage (
                 StandardLib,
                 EFI_VERBOSE_LEVEL_DEFAULT,
                 L"ResetSystem returned unexpectedly."
                 );
  return RecordPreResetFailureAndCleanup (
           RT,
           StandardLib,
           L"ResetSystem returned unexpectedly.",
           (UINTN)__LINE__
           );
}

/**
  Verifies that the authenticated variable is present after reset, checks
  that an unauthenticated update is rejected, and then deletes the variable
  with the required authenticated request.

  @param RT           A pointer to the EFI Runtime Services table.
  @param StandardLib  A pointer to the Standard Test Library protocol.

  @retval EFI_SUCCESS  The final test assertion was recorded.
  @return Other value  The final assertion could not be recorded.
**/
STATIC
EFI_STATUS
VerifyAuthenticatedTestVariableAfterReset (
  IN EFI_RUNTIME_SERVICES                *RT,
  IN EFI_STANDARD_TEST_LIBRARY_PROTOCOL  *StandardLib
  )
{
  EFI_STATUS          Status;
  EFI_STATUS          DeleteStatus;
  EFI_TEST_ASSERTION  Result;
  BOOLEAN             VariablePresent;
  UINT32              Attributes;
  UINTN               VariableDataSize;
  VOID                *VariableData;

  Result = EFI_TEST_ASSERTION_PASSED;
  VariableData = NULL;
  Status = ReadTestVariable (
             RT,
             &VariablePresent,
             &Attributes,
             &VariableDataSize,
             &VariableData
             );
  if (EFI_ERROR (Status)) {
    StandardLib->RecordMessage (
                   StandardLib,
                   EFI_VERBOSE_LEVEL_DEFAULT,
                   L"Could not read the authenticated variable after reset: %r",
                   Status
                   );
    Result = EFI_TEST_ASSERTION_FAILED;
  } else {
    LogTestVariableState (
      StandardLib,
      VariablePresent,
      Attributes,
      VariableDataSize,
      VariableData
      );

    if (!VariablePresent) {
      StandardLib->RecordMessage (
                     StandardLib,
                     EFI_VERBOSE_LEVEL_DEFAULT,
                     L"Authenticated variable is not present after reset."
                     );
      Result = EFI_TEST_ASSERTION_FAILED;
    }
  }

  if (VariableData != NULL) {
    gtBS->FreePool (VariableData);
  }

  if (!EFI_ERROR (Status) && VariablePresent) {
    Status = RT->SetVariable (
                   SECURE_BOOT_AUTH_VARIABLE_NAME,
                   &mSecureBootAuthVariableGuid,
                   SECURE_BOOT_AUTH_VARIABLE_ATTRIBUTES,
                   sizeof (mUnauthenticatedPayload) - 1,
                   (VOID *)mUnauthenticatedPayload
                   );
    if (Status != EFI_SECURITY_VIOLATION) {
      StandardLib->RecordMessage (
                     StandardLib,
                     EFI_VERBOSE_LEVEL_DEFAULT,
                     L"Post-reset unauthenticated update returned %r instead "
                     L"of EFI_SECURITY_VIOLATION.",
                     Status
                     );
      Result = EFI_TEST_ASSERTION_FAILED;
    } else {
      StandardLib->RecordMessage (
                     StandardLib,
                     EFI_VERBOSE_LEVEL_DEFAULT,
                     L"Post-reset unauthenticated update was rejected with "
                     L"EFI_SECURITY_VIOLATION."
                     );
    }
  }

  DeleteStatus = DeleteOwnedTestVariable (
                   RT,
                   StandardLib,
                   gSecureBootAuthVariableDeleteData,
                   gSecureBootAuthVariableDeleteDataSize
                   );
  if (EFI_ERROR (DeleteStatus)) {
    Result = EFI_TEST_ASSERTION_FAILED;
  }

  if (Result == EFI_TEST_ASSERTION_FAILED) {
    return RecordPersistenceAssertion (
             StandardLib,
             EFI_TEST_ASSERTION_FAILED,
             L"Persistence verification, post-reset authentication "
             L"enforcement, or authenticated deletion failed.",
             (UINTN)__LINE__
             );
  }

  return RecordPersistenceAssertion (
           StandardLib,
           EFI_TEST_ASSERTION_PASSED,
           L"The authenticated variable persisted, rejected an "
           L"unauthenticated update after reset, and was deleted.",
           (UINTN)__LINE__
           );
}

/**
  Entry point for the Secure Boot authenticated variable persistence test.

  On the first invocation the test creates and validates its own authenticated
  variable and resets. On recovery it verifies that the variable exists, logs
  its data, verifies that an unauthenticated update is rejected, and performs
  an authenticated deletion.

  @param This             A pointer to the EFI_BB_TEST_PROTOCOL instance.
  @param ClientInterface  A pointer to the EFI Runtime Services interface.
  @param TestLevel        Test thoroughness control.
  @param SupportHandle    A handle containing support protocols.

  @retval EFI_SUCCESS  The test result was recorded or the system reset.
  @return Other value  A required support protocol could not be obtained.
**/
EFI_STATUS
SecureBootAuthVariablePersistenceTest (
  IN EFI_BB_TEST_PROTOCOL  *This,
  IN VOID                  *ClientInterface,
  IN EFI_TEST_LEVEL        TestLevel,
  IN EFI_HANDLE            SupportHandle
  )
{
  EFI_STATUS                          Status;
  EFI_RUNTIME_SERVICES                *RT;
  EFI_STANDARD_TEST_LIBRARY_PROTOCOL  *StandardLib;
  EFI_TEST_RECOVERY_LIBRARY_PROTOCOL  *RecoveryLib;
  EFI_TEST_LOGGING_LIBRARY_PROTOCOL   *LoggingLib;
  UINT8                               ResetRecordBuffer[
                                        SCT_RESET_RECORD_BUFFER_SIZE
                                        ];
  UINTN                               ResetRecordSize;

  Status = GetTestSupportLibrary (
             SupportHandle,
             &StandardLib,
             &RecoveryLib,
             &LoggingLib
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (FALSE == CheckBBTestCanRunAndRecordAssertion (
                  StandardLib,
                  L"Authenticated variable persistence test is not supported in EFI",
                  __FILE__,
                  (UINTN)__LINE__
                  )) {
    return EFI_SUCCESS;
  }

  if (LoggingLib != NULL) {
    LoggingLib->EnterFunction (
                  LoggingLib,
                  L"SecureBootAuthVariablePersistenceTest",
                  L"Secure Boot authenticated variable persistence"
                  );
  }

  (VOID)This;
  (VOID)TestLevel;

  RT = (EFI_RUNTIME_SERVICES *)ClientInterface;
  ResetRecordSize = sizeof (ResetRecordBuffer);
  Status = RecoveryLib->ReadResetRecord (
                          RecoveryLib,
                          &ResetRecordSize,
                          ResetRecordBuffer
                          );
  if (Status == EFI_NOT_FOUND) {
    Status = CreateAuthenticatedTestVariableAndReset (
               RT,
               StandardLib,
               RecoveryLib
               );
  } else if (EFI_ERROR (Status)) {
    StandardLib->RecordMessage (
                   StandardLib,
                   EFI_VERBOSE_LEVEL_DEFAULT,
                   L"Could not read the reset-recovery record: %r",
                   Status
                   );
    Status = RecordPersistenceAssertion (
               StandardLib,
               EFI_TEST_ASSERTION_FAILED,
               L"The SCT reset-recovery record could not be read.",
               (UINTN)__LINE__
               );
  } else if ((ResetRecordSize != 1) ||
             (ResetRecordBuffer[0] != VERIFY_AFTER_RESET_MARKER)) {
    StandardLib->RecordMessage (
                   StandardLib,
                   EFI_VERBOSE_LEVEL_DEFAULT,
                   L"Unexpected reset-recovery marker: size=%d marker=0x%02x",
                   ResetRecordSize,
                   (ResetRecordSize > 0) ? ResetRecordBuffer[0] : 0
                   );
    Status = RecordPersistenceAssertion (
               StandardLib,
               EFI_TEST_ASSERTION_FAILED,
               L"The SCT reset-recovery marker is invalid.",
               (UINTN)__LINE__
               );
  } else {
    Status = VerifyAuthenticatedTestVariableAfterReset (
               RT,
               StandardLib
               );
  }

  if (LoggingLib != NULL) {
    LoggingLib->ExitFunction (
                  LoggingLib,
                  L"SecureBootAuthVariablePersistenceTest",
                  L"Secure Boot authenticated variable persistence"
                  );
  }

  return Status;
}
