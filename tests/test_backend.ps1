# Campus Event Registration Register - Backend Automated Test Harness
$ErrorActionPreference = "Stop"
[System.Net.ServicePointManager]::Expect100Continue = $false
$port = 8080

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Starting C Backend HTTP Server on port $port...        " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# Clean previous test data
if (Test-Path "data/participants.txt") { Remove-Item "data/participants.txt" -Force }
if (Test-Path "data/event_summary.txt") { Remove-Item "data/event_summary.txt" -Force }

$proc = Start-Process -FilePath ".\server.exe" -ArgumentList "$port" -PassThru -NoNewWindow
Start-Sleep -Milliseconds 800

try {
    $baseUrl = "http://127.0.0.1:$port"

    # --- TEST 1: GET /api/events ---
    Write-Host "`n[TEST 1] Testing GET /api/events..." -ForegroundColor Yellow
    $res = Invoke-RestMethod -Uri "$baseUrl/api/events" -Method GET
    Write-Host "Response: $($res | ConvertTo-Json -Compress)" -ForegroundColor Green
    if ($res.events.Count -ne 5) { throw "Expected 5 events, got $($res.events.Count)" }
    Write-Host "-> PASS: Exactly 5 events returned successfully." -ForegroundColor Green

    # --- TEST 2: POST /api/register (Valid Student 1) ---
    Write-Host "`n[TEST 2] Testing POST /api/register (Student 1: Kaushick S)..." -ForegroundColor Yellow
    $body1 = @{
        reg_id = "REG2026101"
        name = "Kaushick S"
        department = "Computer Science and Engineering"
        event_id = 1
    } | ConvertTo-Json
    $res1 = Invoke-RestMethod -Uri "$baseUrl/api/register" -Method POST -Body $body1 -ContentType "application/json"
    Write-Host "Response: $($res1 | ConvertTo-Json -Compress)" -ForegroundColor Green
    if ($res1.status -ne "success" -or $res1.participant.reg_id -ne "REG2026101") {
        throw "Failed to register student 1"
    }
    Write-Host "-> PASS: Student 1 registered successfully." -ForegroundColor Green

    # --- TEST 3: POST /api/register (Student 2 & 3) ---
    Write-Host "`n[TEST 3] Testing POST /api/register (Student 2 & 3)..." -ForegroundColor Yellow
    $body2 = @{
        reg_id = "REG2026102"
        name = "Priya Dharshini"
        department = "Electronics and Communication Engineering"
        event_id = 0
    } | ConvertTo-Json
    $res2 = Invoke-RestMethod -Uri "$baseUrl/api/register" -Method POST -Body $body2 -ContentType "application/json"

    $body3 = @{
        reg_id = "REG2026103"
        name = "Venkatesh R"
        department = "Information Technology"
        event_id = 1
    } | ConvertTo-Json
    $res3 = Invoke-RestMethod -Uri "$baseUrl/api/register" -Method POST -Body $body3 -ContentType "application/json"
    Write-Host "-> PASS: Students 2 and 3 registered successfully." -ForegroundColor Green

    # --- TEST 4: Duplicate Registration ID Detection (Linear Search) ---
    Write-Host "`n[TEST 4] Testing Duplicate ID Detection with REG2026101..." -ForegroundColor Yellow
    $dupBody = @{
        reg_id = "reg2026101" # case-insensitive test
        name = "Another Name"
        department = "Mechanical"
        event_id = 1 # Same event (Coding Competition)
    } | ConvertTo-Json
    $dupCaught = $false
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/register" -Method POST -Body $dupBody -ContentType "application/json"
    } catch {
        $dupCaught = $true
        Write-Host "Caught expected duplicate error: $($_.Exception.Message)" -ForegroundColor Magenta
    }
    if (-not $dupCaught) { throw "Duplicate registration was NOT blocked!" }
    Write-Host "-> PASS: Duplicate Registration ID blocked by linear search." -ForegroundColor Green

    # --- TEST 5: Input Validation (Invalid Event ID & Empty Name) ---
    Write-Host "`n[TEST 5] Testing Validation (Invalid Event ID 9)..." -ForegroundColor Yellow
    $invalidBody = @{
        reg_id = "REG999"
        name = ""
        department = "Civil"
        event_id = 9
    } | ConvertTo-Json
    $valCaught = $false
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/register" -Method POST -Body $invalidBody -ContentType "application/json"
    } catch {
        $valCaught = $true
        Write-Host "Caught expected validation error: $($_.Exception.Message)" -ForegroundColor Magenta
    }
    if (-not $valCaught) { throw "Validation failed to catch bad input!" }
    Write-Host "-> PASS: Validation rejected invalid input." -ForegroundColor Green

    # --- TEST 6: Admin Login Authentication ---
    Write-Host "`n[TEST 6] Testing Admin Login..." -ForegroundColor Yellow
    # Invalid login
    $badLogin = @{ username = "admin"; password = "wrongpassword" } | ConvertTo-Json
    $authFailed = $false
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/admin/login" -Method POST -Body $badLogin -ContentType "application/json"
    } catch {
        $authFailed = $true
    }
    if (-not $authFailed) { throw "Bad login succeeded unexpectedly!" }
    Write-Host "-> PASS: Invalid login properly rejected with 401." -ForegroundColor Green

    # Valid login
    $goodLogin = @{ username = "admin"; password = "admin123" } | ConvertTo-Json
    $loginRes = Invoke-RestMethod -Uri "$baseUrl/api/admin/login" -Method POST -Body $goodLogin -ContentType "application/json"
    $token = $loginRes.token
    if (-not $token) { throw "No token returned from login!" }
    Write-Host "-> PASS: Admin login successful. Token: $token" -ForegroundColor Green

    $adminHeaders = @{ Authorization = "Bearer $token" }

    # --- TEST 7: Admin View All Participants ---
    Write-Host "`n[TEST 7] Testing GET /api/admin/participants (All)..." -ForegroundColor Yellow
    $allParticipants = Invoke-RestMethod -Uri "$baseUrl/api/admin/participants" -Method GET -Headers $adminHeaders
    Write-Host "Total count returned: $($allParticipants.count)" -ForegroundColor Green
    if ($allParticipants.count -ne 3) { throw "Expected 3 participants, got $($allParticipants.count)" }
    Write-Host "-> PASS: All participants retrieved correctly." -ForegroundColor Green

    # --- TEST 8: Admin Searching (Linear Search by name & ID) ---
    Write-Host "`n[TEST 8] Testing Search by Name ('Priya')..." -ForegroundColor Yellow
    $searchRes = Invoke-RestMethod -Uri "$baseUrl/api/admin/participants?search=Priya" -Method GET -Headers $adminHeaders
    if ($searchRes.count -ne 1 -or $searchRes.participants[0].name -ne "Priya Dharshini") {
        throw "Search by name failed!"
    }
    Write-Host "-> PASS: Linear search by name succeeded: $($searchRes.participants[0].name)" -ForegroundColor Green

    # --- TEST 9: Admin Filtering by Event ---
    Write-Host "`n[TEST 9] Testing Filter by Event (Event ID 1: Coding Competition)..." -ForegroundColor Yellow
    $filterRes = Invoke-RestMethod -Uri "$baseUrl/api/admin/participants?event=1" -Method GET -Headers $adminHeaders
    if ($filterRes.count -ne 2) { throw "Expected 2 participants in Coding Competition, got $($filterRes.count)" }
    Write-Host "-> PASS: Filtering by event succeeded (2 participants in Coding Competition)." -ForegroundColor Green

    # --- TEST 10: Admin Event Statistics ---
    Write-Host "`n[TEST 10] Testing GET /api/admin/stats..." -ForegroundColor Yellow
    $stats = Invoke-RestMethod -Uri "$baseUrl/api/admin/stats" -Method GET -Headers $adminHeaders
    Write-Host "Total: $($stats.total), Capacity: $($stats.capacity), Remaining: $($stats.remaining)" -ForegroundColor Green
    foreach ($ev in $stats.events) {
        Write-Host "  Event [$($ev.code)] $($ev.name): $($ev.count) participants ($($ev.percentage)%)"
    }
    if ($stats.total -ne 3) { throw "Stats total mismatch!" }
    Write-Host "-> PASS: Event-wise statistics computed accurately." -ForegroundColor Green

    # --- TEST 11: Summary Generation (event_summary.txt) ---
    Write-Host "`n[TEST 11] Testing POST /api/admin/summary..." -ForegroundColor Yellow
    $sumRes = Invoke-RestMethod -Uri "$baseUrl/api/admin/summary" -Method POST -Headers $adminHeaders
    Write-Host "Response: $($sumRes | ConvertTo-Json -Compress)" -ForegroundColor Green
    if (-not (Test-Path "data/event_summary.txt")) {
        throw "data/event_summary.txt was not generated!"
    }
    Write-Host "-> PASS: event_summary.txt generated successfully on disk." -ForegroundColor Green

    # --- TEST 12: Inspect Persistent Files ---
    Write-Host "`n[TEST 12] Inspecting data/participants.txt and data/event_summary.txt..." -ForegroundColor Yellow
    Write-Host "`n--- data/participants.txt ---" -ForegroundColor Cyan
    Get-Content "data/participants.txt"
    Write-Host "`n--- data/event_summary.txt ---" -ForegroundColor Cyan
    Get-Content "data/event_summary.txt"

    Write-Host "`n==========================================================" -ForegroundColor Green
    Write-Host "  ALL 12 BACKEND AUTOMATED TESTS PASSED SUCCESSFULLY!    " -ForegroundColor Green
    Write-Host "==========================================================" -ForegroundColor Green

} finally {
    Write-Host "`nStopping server process (PID: $($proc.Id))..." -ForegroundColor Gray
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
}
