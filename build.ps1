# build.ps1 - Script de compilacion para Project1
# Uso: .\build.ps1           (Debug por defecto)
#      .\build.ps1 -Release  (Optimizado)
param([switch]$Release)

$Root   = $PSScriptRoot
$Out    = "$Root\build\Debug\outDebug.exe"
$Flags  = "-g -O0 -Wall"

if ($Release) {
    $Out   = "$Root\build\Release\outRelease.exe"
    $Flags = "-O2 -Wall -DNDEBUG"
}

# Crear directorio de salida si no existe
New-Item -ItemType Directory -Force -Path (Split-Path $Out) | Out-Null

$Sources = @(
    "$Root\main.cpp",
    "$Root\Enemy.cpp",
    "$Root\Reaper.cpp",
    "$Root\Ropera.cpp",
    "$Root\scenes\MainMenuScene.cpp",
    "$Root\scenes\CharacterSelectScene.cpp",
    "$Root\scenes\GameplayScene.cpp",
    "$Root\scenes\PauseScene.cpp",
    "$Root\scenes\SettingsScene.cpp"
)

$Cmd = "g++ $Flags $($Sources -join ' ') -o `"$Out`" -I. -lraylib -lgdi32 -lwinmm"
Write-Host "Compilando..." -ForegroundColor Cyan
Write-Host $Cmd -ForegroundColor DarkGray

Invoke-Expression $Cmd

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n[OK] Compilacion exitosa -> $Out" -ForegroundColor Green
    
    # Crear/Actualizar acceso directo en el Escritorio
    try {
        $DesktopPath = [System.IO.Path]::Combine($env:USERPROFILE, "Desktop", "Project1.lnk")
        $WshShell = New-Object -ComObject WScript.Shell
        $Shortcut = $WshShell.CreateShortcut($DesktopPath)
        $Shortcut.TargetPath = $Out
        $Shortcut.WorkingDirectory = (Split-Path $Out) # Para que encuentre assets y DLLs
        $Shortcut.Save()
        Write-Host "[INFO] Acceso directo actualizado en el Escritorio." -ForegroundColor Cyan
    } catch {
        Write-Host "[WARN] No se pudo crear el acceso directo: $($_.Exception.Message)" -ForegroundColor Yellow
    }
} else {
    Write-Host "`n[ERROR] Compilacion fallida (exit $LASTEXITCODE)" -ForegroundColor Red
}
