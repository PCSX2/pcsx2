@echo off

set OutDirFullPath=%1
set ProjectDir=%2

ECHO "Running Post-Build Event"
ECHO "%OutDirFullPath%"
ECHO "%ProjectDir%"
IF EXIST "%OutDirFullPath%portable.build.yaml" (
  ECHO "bin\portable.build.yaml file detected, using it instead of Docs\portable.yaml!"
  COPY  "%OutDirFullPath%portable.build.yaml" "%OutDirFullPath%portable.yaml"
) ELSE (
  ECHO "Copying Docs\portable.yaml file into bin\"
  IF EXIST "%ProjectDir%Docs\portable.yaml" (
    COPY "%ProjectDir%Docs\portable.yaml" "%OutDirFullPath%portable.yaml"
  ) ELSE (
    ECHO "Docs\portable.yaml file not found!"
  )
)
