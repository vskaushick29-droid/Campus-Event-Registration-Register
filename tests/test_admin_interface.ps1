# Test Admin Frontend & C Backend Integration Flow
$ErrorActionPreference = "Stop"
[System.Net.ServicePointManager]::Expect100Continue = $false
$port = 8080

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Starting C Backend HTTP Server on port $port...        " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

$proc = Start-Process -FilePath ".\server.exe" -ArgumentList "$port" -PassThru -NoNewWindow
Start-Sleep -Milliseconds 800

try {
    $baseUrl = "http://127.0.0.1:$port"

    # --- TEST 1: Serve Admin HTML Page ---
    Write-Host "`n[TEST 1] Fetching GET /admin (web/admin.html)..." -ForegroundColor Yellow
    $htmlRes = Invoke-WebRequest -Uri "$baseUrl/admin" -Method GET -UseBasicParsing
    if ($htmlRes.StatusCode -ne 200) { throw "Failed to load admin.html" }
    if (-not $htmlRes.Content.Contains("Admin Dashboard")) {
        throw "admin.html content missing title!"
    }
    if (-not $htmlRes.Content.Contains("Confirmation No")) {
        throw "admin.html content missing Confirmation No column header!"
    }
    Write-Host "-> PASS: admin.html served with HTTP 200 OK (Length: $($htmlRes.RawContentLength) bytes)" -ForegroundColor Green

    # --- TEST 2: Serve Admin CSS ---
    Write-Host "`n[TEST 2] Fetching GET /css/admin.css..." -ForegroundColor Yellow
    $cssRes = Invoke-WebRequest -Uri "$baseUrl/css/admin.css" -Method GET -UseBasicParsing
    if ($cssRes.StatusCode -ne 200) { throw "Failed to load admin.css" }
    if (-not $cssRes.Content.Contains(".events-stats-grid")) {
        throw "admin.css content invalid!"
    }
    Write-Host "-> PASS: admin.css served with HTTP 200 OK (Length: $($cssRes.RawContentLength) bytes)" -ForegroundColor Green

    # --- TEST 3: Serve Admin JavaScript ---
    Write-Host "`n[TEST 3] Fetching GET /js/admin.js..." -ForegroundColor Yellow
    $jsRes = Invoke-WebRequest -Uri "$baseUrl/js/admin.js" -Method GET -UseBasicParsing
    if ($jsRes.StatusCode -ne 200) { throw "Failed to load admin.js" }
    if (-not $jsRes.Content.Contains("campus_admin_token")) {
        throw "admin.js content invalid!"
    }
    Write-Host "-> PASS: admin.js served with HTTP 200 OK (Length: $($jsRes.RawContentLength) bytes)" -ForegroundColor Green

    # --- TEST 4: Admin Authentication via C Backend ---
    Write-Host "`n[TEST 4] Testing Admin Login (POST /api/admin/login)..." -ForegroundColor Yellow
    # Invalid password test
    $badCreds = @{ username = "admin"; password = "wrong" } | ConvertTo-Json
    $badAuthBlocked = $false
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/admin/login" -Method POST -Body $badCreds -ContentType "application/json"
    } catch {
        $badAuthBlocked = $true
        Write-Host "Caught expected invalid password: $($_.Exception.Message)" -ForegroundColor Magenta
    }
    if (-not $badAuthBlocked) { throw "Invalid credentials were not rejected!" }

    # Valid login test
    $goodCreds = @{ username = "admin"; password = "admin123" } | ConvertTo-Json
    $loginRes = Invoke-RestMethod -Uri "$baseUrl/api/admin/login" -Method POST -Body $goodCreds -ContentType "application/json"
    if ($loginRes.status -ne "success" -or -not $loginRes.token) {
        throw "Failed to authenticate valid admin!"
    }
    $token = $loginRes.token
    Write-Host "-> PASS: Admin successfully authenticated. Token: $token" -ForegroundColor Green
    $adminHeaders = @{ Authorization = "Bearer $token" }

    # --- TEST 5: Unauthorized Access Protection ---
    Write-Host "`n[TEST 5] Testing Unauthorized Protection without Token..." -ForegroundColor Yellow
    $unauthBlocked = $false
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/admin/participants" -Method GET
    } catch {
        $unauthBlocked = $true
        Write-Host "Caught expected unauthorized block: $($_.Exception.Message)" -ForegroundColor Magenta
    }
    if (-not $unauthBlocked) { throw "Endpoint allowed unauthenticated access!" }
    Write-Host "-> PASS: Unauthenticated access blocked with HTTP 401." -ForegroundColor Green

    # --- TEST 6: Load Dashboard Statistics ---
    Write-Host "`n[TEST 6] Testing GET /api/admin/stats..." -ForegroundColor Yellow
    $stats = Invoke-RestMethod -Uri "$baseUrl/api/admin/stats" -Method GET -Headers $adminHeaders
    Write-Host "Total: $($stats.total) | Capacity: $($stats.capacity) | Remaining: $($stats.remaining)" -ForegroundColor Cyan
    if ($stats.capacity -ne 500) { throw "Capacity was not 500!" }
    if ($stats.events.Count -ne 5) { throw "Expected 5 event stat items!" }
    Write-Host "-> PASS: Dashboard metrics and all 5 event percentages calculated in C." -ForegroundColor Green

    # --- TEST 7: Load Participants Table ---
    Write-Host "`n[TEST 7] Testing GET /api/admin/participants (All)..." -ForegroundColor Yellow
    $all = Invoke-RestMethod -Uri "$baseUrl/api/admin/participants" -Method GET -Headers $adminHeaders
    if ($all.count -eq 0) {
        Write-Host "Database is fresh/empty. Registering sample record for search/filter tests..." -ForegroundColor Cyan
        $initBody = @{ reg_id = "REG2026101"; name = "Kaushick S"; department = "Computer Science and Engineering"; event_id = 1 } | ConvertTo-Json
        Invoke-RestMethod -Uri "$baseUrl/api/register" -Method POST -Body $initBody -ContentType "application/json" | Out-Null
        $all = Invoke-RestMethod -Uri "$baseUrl/api/admin/participants" -Method GET -Headers $adminHeaders
    }
    Write-Host "Total participants in C database: $($all.count)" -ForegroundColor Cyan
    if ($all.participants.Count -lt 1) { throw "Participants list is unexpectedly empty!" }
    Write-Host "Sample participant: $($all.participants[0].reg_id) - $($all.participants[0].name) ($($all.participants[0].event_name)) [CNF: $($all.participants[0].confirmation_no)]" -ForegroundColor Cyan
    if (-not $all.participants[0].confirmation_no -or -not $all.participants[0].confirmation_no.StartsWith("CNF")) {
        throw "Participant confirmation number missing or invalid format: $($all.participants[0].confirmation_no)"
    }
    Write-Host "-> PASS: Complete participant table loaded with confirmation numbers from C backend." -ForegroundColor Green

    # --- TEST 8: Live Search by Reg ID or Name (Linear Search in C) ---
    Write-Host "`n[TEST 8] Testing Search by Reg ID..." -ForegroundColor Yellow
    $testTarget = $all.participants[0].reg_id
    $searchRes = Invoke-RestMethod -Uri "$baseUrl/api/admin/participants?search=$testTarget" -Method GET -Headers $adminHeaders
    if ($searchRes.count -ne 1 -or $searchRes.participants[0].reg_id -ne $testTarget) {
        throw "Linear search by Reg ID failed!"
    }
    Write-Host "-> PASS: C Linear search found: $($searchRes.participants[0].name) [$testTarget]" -ForegroundColor Green

    # --- TEST 8B: Live Search by Confirmation Number ---
    Write-Host "`n[TEST 8B] Testing Search by Confirmation Number..." -ForegroundColor Yellow
    $testCnf = $all.participants[0].confirmation_no
    $searchCnfRes = Invoke-RestMethod -Uri "$baseUrl/api/admin/participants?search=$testCnf" -Method GET -Headers $adminHeaders
    if ($searchCnfRes.count -lt 1 -or $searchCnfRes.participants[0].confirmation_no -ne $testCnf) {
        throw "Linear search by Confirmation Number failed!"
    }
    Write-Host "-> PASS: C Linear search found student by Confirmation Number: $($searchCnfRes.participants[0].name) [$testCnf]" -ForegroundColor Green

    # --- TEST 9: Event Filter ---
    Write-Host "`n[TEST 9] Testing Filter by Event ID 1 (Coding Competition)..." -ForegroundColor Yellow
    $filterRes = Invoke-RestMethod -Uri "$baseUrl/api/admin/participants?event=1" -Method GET -Headers $adminHeaders
    foreach ($p in $filterRes.participants) {
        if ($p.event_id -ne 1) { throw "Filter returned non-matching event!" }
    }
    Write-Host "-> PASS: Filter by event returned $($filterRes.count) participant(s), all matching event_id=1." -ForegroundColor Green

    # --- TEST 10: Generate and Fetch Summary Report ---
    Write-Host "`n[TEST 10] Testing POST /api/admin/summary and GET /api/admin/summary..." -ForegroundColor Yellow
    $genRes = Invoke-RestMethod -Uri "$baseUrl/api/admin/summary" -Method POST -Headers $adminHeaders
    if ($genRes.status -ne "success") { throw "Failed to generate summary report!" }

    $viewRes = Invoke-RestMethod -Uri "$baseUrl/api/admin/summary" -Method GET -Headers $adminHeaders
    if ($viewRes.status -ne "success" -or -not $viewRes.content.Contains("CAMPUS EVENT REGISTRATION SUMMARY REPORT")) {
        throw "Failed to fetch formatted summary content!"
    }
    Write-Host "-> PASS: Summary report generated on disk and read by admin modal." -ForegroundColor Green

    Write-Host "`n==========================================================" -ForegroundColor Green
    Write-Host "  ADMIN INTERFACE INTEGRATION TESTS ALL PASSED!          " -ForegroundColor Green
    Write-Host "==========================================================" -ForegroundColor Green

} finally {
    Write-Host "`nStopping server process (PID: $($proc.Id))..." -ForegroundColor Gray
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
}
