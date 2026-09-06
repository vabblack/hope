# PowerShell Authenticode Code Signing Script for Hope & Watchdog
param(
    [string[]]$Binaries = @("Release\watchdog.exe", "Release\BackgroundHost.exe", "Release\hope.exe"),
    [string]$CertSubject = "CN=Hope App Code Signing",
    [string]$TimestampServer = "http://timestamp.digicert.com"
)

$ErrorActionPreference = "Stop"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   Authenticode Code Signing Pipeline for Windows" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Locate SignTool.exe
Write-Host "`n[1/4] Locating signtool.exe..." -ForegroundColor Yellow
$signtool = (Get-Command signtool.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source)
if (-not $signtool) {
    $sdkPaths = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin\*\x64\signtool.exe" -ErrorAction SilentlyContinue | Sort-Object FullName -Descending
    if ($sdkPaths) {
        $signtool = $sdkPaths[0].FullName
    }
}

if (-not $signtool -or -not (Test-Path $signtool)) {
    Write-Error "signtool.exe could not be found. Please ensure Windows 10/11 SDK is installed."
    exit 1
}
Write-Host "Found signtool: $signtool" -ForegroundColor Green

# 2. Check or Create Code Signing Certificate
Write-Host "`n[2/4] Verifying Code Signing Certificate..." -ForegroundColor Yellow
$cert = Get-ChildItem "Cert:\CurrentUser\My" | Where-Object { $_.Subject -eq $CertSubject } | Select-Object -First 1

if (-not $cert) {
    Write-Host "Certificate '$CertSubject' not found in CurrentUser\My. Generating new SHA-256 certificate..." -ForegroundColor Cyan
    $cert = New-SelfSignedCertificate `
        -Type CodeSigningCert `
        -Subject $CertSubject `
        -CertStoreLocation "Cert:\CurrentUser\My" `
        -HashAlgorithm SHA256 `
        -KeyLength 2048 `
        -NotAfter (Get-Date).AddYears(5)
    
    Write-Host "Certificate generated with thumbprint: $($cert.Thumbprint)" -ForegroundColor Green
} else {
    Write-Host "Existing certificate found with thumbprint: $($cert.Thumbprint)" -ForegroundColor Green
}

# Export public CER and backup PFX
$cerPath = Join-Path $PSScriptRoot "HopeSigningCert.cer"
if (-not (Test-Path $cerPath)) {
    $cerBytes = $cert.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Cert)
    [System.IO.File]::WriteAllBytes($cerPath, $cerBytes)
    Write-Host "Exported public certificate: $cerPath" -ForegroundColor Gray
}

# Export PFX for backup / external usage
$pfxPath = Join-Path $PSScriptRoot "HopeSigningCert.pfx"
if (-not (Test-Path $pfxPath)) {
    $pwd = ConvertTo-SecureString -String "HopePassword123!" -Force -AsPlainText
    Export-PfxCertificate -Cert $cert -FilePath $pfxPath -Password $pwd | Out-Null
    Write-Host "Exported backup PFX: $pfxPath (Password: HopePassword123!)" -ForegroundColor Gray
}

# 3. Sign Binaries
Write-Host "`n[3/4] Signing target binaries..." -ForegroundColor Yellow
foreach ($relPath in $Binaries) {
    $targetPath = if ([System.IO.Path]::IsPathRooted($relPath)) { $relPath } else { Join-Path $PSScriptRoot $relPath }
    
    if (-not (Test-Path $targetPath)) {
        Write-Warning "Skipping '$targetPath' - file does not exist yet."
        continue
    }

    Write-Host "Signing: $targetPath" -ForegroundColor Cyan
    
    # Try signing with RFC 3161 timestamping first
    $signOutput = & $signtool sign /sha1 $cert.Thumbprint /fd SHA256 /tr $TimestampServer /td SHA256 "$targetPath" 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Timestamp server failed or unreachable. Signing without timestamp..."
        $signOutput = & $signtool sign /sha1 $cert.Thumbprint /fd SHA256 "$targetPath" 2>&1
    }
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host " Successfully signed: $targetPath" -ForegroundColor Green
    } else {
        Write-Error "Failed to sign $targetPath : $signOutput"
    }
}

# 4. Verify Signatures
Write-Host "`n[4/4] Verifying Authenticode Signatures..." -ForegroundColor Yellow
foreach ($relPath in $Binaries) {
    $targetPath = if ([System.IO.Path]::IsPathRooted($relPath)) { $relPath } else { Join-Path $PSScriptRoot $relPath }
    if (Test-Path $targetPath) {
        Write-Host "`nVerifying: $targetPath" -ForegroundColor Cyan
        $sigInfo = Get-AuthenticodeSignature $targetPath
        Write-Host "  Status       : $($sigInfo.Status)" -ForegroundColor $(if ($sigInfo.Status -eq "Valid") { "Green" } else { "Yellow" })
        Write-Host "  Signer       : $($sigInfo.SignerCertificate.Subject)"
        Write-Host "  Thumbprint   : $($sigInfo.SignerCertificate.Thumbprint)"
        Write-Host "  Digest Algo  : $($sigInfo.SignerCertificate.SignatureAlgorithm.FriendlyName)"
    }
}

Write-Host "`n==========================================================" -ForegroundColor Green
Write-Host "   Code Signing Process Complete!" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green
