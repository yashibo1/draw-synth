;------------------------------------------------------------------------------
; DrawSynth Windows installer (NSIS)
;
; This script expects a Release build to already exist one directory up, at
; ../build/VST3/DrawSynth.vst3/... and (optionally) ../build/Standalone/DrawSynth.exe
; - i.e. it packages binaries that were already built with real MSVC, it does
; not build them itself. See:
;   - .github/workflows/build-installer.yml, which builds those binaries on a
;     real Windows runner and compiles this script into DrawSynth-Setup.exe
;     automatically, or
;   - README.md, "Building the Windows installer yourself", for the exact
;     local commands (Visual Studio + NSIS, no GitHub required).
;
; Compile with:  makensis installer/DrawSynth-Setup.nsi
;------------------------------------------------------------------------------

Unicode true

!include "MUI2.nsh"

Name "DrawSynth"
OutFile "DrawSynth-Setup.exe"
InstallDir "$COMMONFILES64\VST3\DrawSynth.vst3"
RequestExecutionLevel admin

;--------------------------------
; UI

!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

;--------------------------------
; Installed sections

Section "VST3 Plugin (required)" SecVST3
    SectionIn RO
    SetShellVarContext all

    SetOutPath "$COMMONFILES64\VST3\DrawSynth.vst3\Contents\x86_64-win"
    File "..\build\DrawSynth_artefacts\Release\VST3\DrawSynth.vst3\Contents\x86_64-win\DrawSynth.vst3"

    SetOutPath "$COMMONFILES64\VST3\DrawSynth.vst3\Contents\Resources"
    File "..\build\DrawSynth_artefacts\Release\VST3\DrawSynth.vst3\Contents\Resources\moduleinfo.json"

    SetOutPath "$COMMONFILES64\VST3"
    WriteUninstaller "$COMMONFILES64\VST3\DrawSynth-Uninstall.exe"

    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\DrawSynth" "DisplayName" "DrawSynth"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\DrawSynth" "UninstallString" "$COMMONFILES64\VST3\DrawSynth-Uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\DrawSynth" "DisplayVersion" "1.0.0"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\DrawSynth" "Publisher" "DrawSynth Project"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\DrawSynth" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\DrawSynth" "NoRepair" 1
SectionEnd

Section "Standalone Application" SecStandalone
    SetShellVarContext all
    SetOutPath "$PROGRAMFILES64\DrawSynth"
    File "..\build\DrawSynth_artefacts\Release\Standalone\DrawSynth.exe"

    CreateDirectory "$SMPROGRAMS\DrawSynth"
    CreateShortcut "$SMPROGRAMS\DrawSynth\DrawSynth.lnk" "$PROGRAMFILES64\DrawSynth\DrawSynth.exe"
    CreateShortcut "$SMPROGRAMS\DrawSynth\Uninstall.lnk" "$COMMONFILES64\VST3\DrawSynth-Uninstall.exe"
SectionEnd

;--------------------------------
; Component descriptions

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecVST3} "The DrawSynth VST3 plugin, installed to the shared VST3 folder every VST3 host (including FL Studio) scans automatically."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecStandalone} "A standalone version of DrawSynth you can run without a DAW, for quick sketching."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
; Uninstaller

Section "Uninstall"
    SetShellVarContext all
    RMDir /r "$COMMONFILES64\VST3\DrawSynth.vst3"
    Delete "$COMMONFILES64\VST3\DrawSynth-Uninstall.exe"
    RMDir /r "$PROGRAMFILES64\DrawSynth"
    RMDir /r "$SMPROGRAMS\DrawSynth"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\DrawSynth"
SectionEnd
