# sweep.ps1 - run m65c02recomp over a folder of .lnx ROMs as a corpus.
#
# The whole Lynx library is the test set: the recompiler is only as good as the
# worst ROM it chokes on. Phase 1 this just exercises `info` (header parse) over
# every ROM and reports; later phases extend it to recompile + compile-check,
# emitting a STATUS table the way vbrecomp does.
#
# Usage: pwsh scripts/sweep.ps1 -RomDir <dir> [-Exe <m65c02recomp.exe>]
param(
    [string]$RomDir = "roms",
    [string]$Exe = "build/tools/m65c02recomp/Release/m65c02recomp.exe"
)

if (-not (Test-Path $Exe)) { throw "recompiler not found at $Exe - build first" }
$roms = Get-ChildItem -Path $RomDir -Filter *.lnx -Recurse -ErrorAction SilentlyContinue
if (-not $roms) { Write-Host "no .lnx ROMs under $RomDir"; return }

$ok = 0
foreach ($r in $roms) {
    $out = & $Exe info "$($r.FullName)" 2>&1
    $name = ($out | Select-String 'cart name\s*:\s*(.+)$').Matches.Groups[1].Value
    if ($LASTEXITCODE -eq 0) { $ok++; $status = "ok" } else { $status = "FAIL" }
    "{0,-5} {1,-40} {2}" -f $status, $r.Name, $name
}
Write-Host ""
Write-Host ("parsed {0}/{1} ROM headers" -f $ok, $roms.Count)
