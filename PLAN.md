Windows Updates Settings Page Implementation Plan

1. Required feature

Add a Windows Updates section to the existing Settings window.

This section must allow the user to:

1. Open a Windows Updates tab/page from Settings.
2. Search for available Windows software updates.
3. View pending updates and available metadata.
4. Select one or more updates using checkboxes.
5. Install the selected updates.
6. See install, reboot, and verification status.

The implementation must not depend on Windows Settings or Explorer.

2. Settings UI requirements

Add a new Settings tab/page named:

Windows Updates

Use a restart/update-style icon for the tab. Use a matching accent color consistent with the app’s existing visual language.

The page layout must contain:

Top bar:
  Left:  Search for updates button
  Right: Install selected button
Main content:
  Update list with checkboxes
  Metadata visible per update
  Status/progress area

3. Initial UI state

When the Windows Updates page opens:

Search for updates button: enabled
Install selected button: disabled / grayed out
Update list: empty
Status text: neutral idle state

Suggested idle text:

No scan has been run yet.

The app must not auto-install anything when the page opens.

4. Search behavior

When the user clicks Search for updates:

1. Disable the search button while scanning.
2. Clear any stale list contents unless preserving a previous result is part of the app’s established UX pattern.
3. Start a Windows Update Agent scan using:

IsInstalled=0 and IsHidden=0 and Type='Software'

4. When scan completes, populate the update list.
5. Re-enable the search button.
6. Keep Install selected disabled until at least one update is checked.

Drivers must not be included in the default scan.

The app must use the machine’s configured update source by default. Do not force Windows Update over WSUS or another managed update source.

5. Update list item requirements

Each update row/card must include a checkbox and must expose the following information when available:

Title
KB number(s)
Categories
Selected reason
Downloaded state
Reboot requirement if known
UpdateID
RevisionNumber
Install/verification state

The UI may show a compact row by default, but the metadata must be available either inline or through an expandable/details region.

Minimum visible row content:

[checkbox] Title
KB: <KBs>
Category: <categories>
Status: <current state>

6. Checkbox behavior

Each pending update must have an independent checkbox.

When zero updates are checked:

Install selected button: disabled / grayed out

When one or more updates are checked:

Install selected button: enabled

The user must be able to select one update or multiple updates.

The user must be able to deselect updates before installation starts.

Once installation starts, selected update checkboxes must be disabled until the operation finishes or fails.

7. Default selection policy

After scan, the app must automatically mark security-maintenance updates as selected by default.

The default policy is:

SecurityMaintenance

Under this policy, select updates matching any of the following:

Category contains Security Updates
Category contains Critical Updates
Category contains Definition Updates
Category contains Update Rollups
Category contains Windows Security platform
Category contains Microsoft Defender Antivirus
Title contains Security Update
Title contains Cumulative Update
Title contains Servicing Stack
Title contains Windows Security platform
Title contains Microsoft Defender
Title contains Security Intelligence Update
Title contains Definition Update
Title contains Malicious Software Removal Tool

The app must not rely on MsrcSeverity alone. Some security-relevant updates may not provide that field.

8. Install button behavior

The Install selected button must remain disabled until at least one update is selected.

When clicked:

1. Check whether the process/worker performing installation is elevated.
2. If not elevated, request elevation before download/install.
3. Do not attempt download/install without elevation.
4. Re-scan before installation.
5. Re-resolve selected updates using UpdateID and RevisionNumber.
6. Build the Windows Update Agent update collection.
7. Download selected updates.
8. Install selected updates.
9. Show per-update result state.
10. If reboot is required, show reboot-required status.
11. After reboot, verify final installation state.

Do not persist or pass Windows Update Agent COM update objects between phases or processes.

Persist only scalar identity:

UpdateID
RevisionNumber
KBArticleIDs
Title

9. Required elevation behavior

Scanning may run unelevated.

Download and install must require an elevated administrator token.

If not elevated, the app must return or display:

Administrator permission is required to install Windows updates.

Internal structured error:

ErrorClass: NotElevated
Associated HRESULT: 0x80240044
Meaning: per-machine update access denied
Required action: relaunch elevated

When both overall and per-update HRESULTs are available, the per-update HRESULT must be used as the primary diagnostic for that specific update.

10. Required backend implementation

Implement the Windows Update functionality natively using Windows Update Agent COM.

Required API surface:

IUpdateSession
IUpdateSearcher
ISearchResult
IUpdateCollection
IUpdate
IUpdateDownloader
IUpdateInstaller
IUpdateDownloadResult
IUpdateInstallationResult
WUA history APIs exposed by the searcher

Required native references:

wuapi.h
Wuapi.dll
Wuguid.lib

11. Required install flow

For each install operation, perform this exact flow:

1. Start new Windows Update Agent session.
2. Run fresh scan.
3. Match selected updates by UpdateID + RevisionNumber.
4. If a selected update is no longer found, mark it SupersededOrNoLongerApplicable.
5. Accept EULA if needed.
6. Add matched updates to IUpdateCollection.
7. Download collection.
8. Record overall download result.
9. Record per-update download results.
10. If any selected update fails download, do not install that failed update.
11. Install successfully downloaded updates.
12. Record overall install result.
13. Record per-update install results.
14. Record reboot requirement.
15. Rescan.
16. Verify pending state.
17. If reboot required, mark InstalledRebootRequired and wait for post-reboot verification.

12. Required update states

Use these states:

Discovered
Selected
Downloading
Downloaded
Installing
InstalledNoRebootRequired
InstalledRebootRequired
RebootObserved
VerifiedInstalled
Failed

Use these failure classes:

NotElevated
DownloadFailed
InstallFailed
VerificationFailed
RebootRequiredBeforeInstall
SupersededOrNoLongerApplicable
PolicyBlocked
AnotherOperationInProgress

RebootRequired = true is not a failure.

13. Required verification rules

For non-OS security-maintenance updates, final verification requires:

Windows Update Agent install result succeeded
Windows Update Agent history contains success
Target update no longer appears in pending scan

For cumulative OS/security updates, final verification requires:

Windows Update Agent install result succeeded
If reboot required, reboot occurred
Post-reboot Windows Update Agent history contains success
Target update no longer appears in pending scan
OS image/build/package state reflects the installed update where available

If an OS update still appears pending before reboot, do not mark it failed if install succeeded and reboot is required.

If Windows Update Agent history reports 0x80242014 before reboot, treat it as post-reboot work still pending.

14. Required result-code handling

Map at least these values:

ResultCode 2
Meaning: succeeded
ResultCode 3
Meaning: succeeded with errors
ResultCode 4
Meaning: failed
ResultCode 5
Meaning: aborted/cancelled
HRESULT 0x00000000
Meaning: success
HRESULT 0x80240022
Meaning: all updates in the operation failed
HRESULT 0x80240044
Meaning: per-machine update access denied; elevation required
HRESULT 0x80242014
Meaning: post-reboot update operation still pending

Store both overall and per-update results.

15. Required UI status labels

The UI must distinguish these states clearly:

Not scanned
Scanning
Available
Selected
Downloading
Installing
Installed
Installed - restart required
Verifying
Verified installed
Failed

For an install that succeeds but requires reboot, show:

Installed - restart required

Do not show it as failed.

After reboot verification succeeds, show:

Verified installed

16. Required structured result fields

Each install operation must produce structured data with:

Operation
StartedUtc
CompletedUtc
OverallDownloadResultCode
OverallDownloadHResult
OverallInstallResultCode
OverallInstallHResult
OverallRebootRequired
PerUpdateResults

Each per-update result must include:

UpdateID
RevisionNumber
KBArticleIDs
Title
SelectedReason
DownloadResultCode
DownloadHResult
InstallResultCode
InstallHResult
RebootRequired
VerificationStatus
FinalState

17. Functional acceptance requirements

The feature is complete only when the following behaviors work:

Scan behavior

Expected:

User opens Settings > Windows Updates.
User clicks Search for updates.
App lists pending software updates.
Install selected is disabled until at least one checkbox is selected.
Security-maintenance updates are selected by default.

Non-elevated install behavior

Expected:

Install attempt requests elevation before download/install.
No non-elevated download/install is attempted.
If elevation is unavailable, state is NotElevated.

Single update install behavior

Expected:

A selected update downloads successfully.
A selected update installs successfully.
Per-update result is shown.
Final state is VerifiedInstalled, InstalledNoRebootRequired, or InstalledRebootRequired depending on result.

Multiple update install behavior

Expected:

Multiple selected updates can be downloaded and installed in one operation.
Each update receives its own per-update result.
One update’s result must not overwrite or obscure another update’s result.

Reboot-required behavior

Expected:

If an update installs successfully but requires reboot, show Installed - restart required.
Do not mark reboot-required installs as failed.
After reboot, verify again and update the state to VerifiedInstalled if verification succeeds.

18. Explicit non-goals

Do not implement driver updates in the default flow.

Do not depend on Windows Settings.

Do not depend on Explorer.

Do not use KB number alone as the primary identity.

Do not treat blank MsrcSeverity as non-security.

Do not treat reboot-required as failure.

Do not persist or reuse Windows Update Agent COM objects across runs.
