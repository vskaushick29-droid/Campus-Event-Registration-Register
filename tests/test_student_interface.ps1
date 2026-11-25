# Test Student Frontend & C Backend Integration Flow
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

    # --- TEST 1: Serve Student HTML Page ---
    Write-Host "`n[TEST 1] Fetching GET / (web/index.html)..." -ForegroundColor Yellow
    $htmlRes = Invoke-WebRequest -Uri "$baseUrl/" -Method GET -UseBasicParsing
    if ($htmlRes.StatusCode -ne 200) { throw "Failed to load index.html" }
    if (-not $htmlRes.Content.Contains("Campus Event Registration Portal")) {
        throw "index.html content missing title!"
    }
    Write-Host "-> PASS: index.html served with HTTP 200 OK (Length: $($htmlRes.RawContentLength) bytes)" -ForegroundColor Green

    # --- TEST 2: Serve Student CSS ---
    Write-Host "`n[TEST 2] Fetching GET /css/student.css..." -ForegroundColor Yellow
    $cssRes = Invoke-WebRequest -Uri "$baseUrl/css/student.css" -Method GET -UseBasicParsing
    if ($cssRes.StatusCode -ne 200) { throw "Failed to load student.css" }
    if (-not $cssRes.Content.Contains(".confirmation-card")) {
        throw "student.css content invalid!"
    }
    Write-Host "-> PASS: student.css served with HTTP 200 OK (Length: $($cssRes.RawContentLength) bytes)" -ForegroundColor Green

    # --- TEST 3: Serve Student JavaScript ---
    Write-Host "`n[TEST 3] Fetching GET /js/student.js..." -ForegroundColor Yellow
    $jsRes = Invoke-WebRequest -Uri "$baseUrl/js/student.js" -Method GET -UseBasicParsing
    if ($jsRes.StatusCode -ne 200) { throw "Failed to load student.js" }
    if (-not $jsRes.Content.Contains("loadAvailableEvents")) {
        throw "student.js content invalid!"
    }
    Write-Host "-> PASS: student.js served with HTTP 200 OK (Length: $($jsRes.RawContentLength) bytes)" -ForegroundColor Green

    # --- TEST 4: Student UI Flow: Load Events from Backend ---
    Write-Host "`n[TEST 4] Fetching GET /api/events (as student.js does)..." -ForegroundColor Yellow
    $eventsRes = Invoke-RestMethod -Uri "$baseUrl/api/events" -Method GET
    if ($eventsRes.status -ne "success" -or $eventsRes.events.Count -ne 5) {
        throw "Failed to fetch 5 events"
    }
    Write-Host "Loaded Events:" -ForegroundColor Cyan
    foreach ($ev in $eventsRes.events) {
        Write-Host "  [$($ev.id)] $($ev.name) ($($ev.code))"
    }
    Write-Host "-> PASS: Events API loaded successfully for student dropdown." -ForegroundColor Green

    # --- TEST 5: Complete Student Registration Submission Flow ---
    Write-Host "`n[TEST 5] Submitting Student Registration (POST /api/register)..." -ForegroundColor Yellow
    $uniqueId = "STU" + (Get-Random -Minimum 1000 -Maximum 9999)
    $studentPayload = @{
        reg_id = $uniqueId
        name = "Kavitha Sundaram"
        department = "Artificial Intelligence and Data Science"
        event_id = 3 # Quiz Competition
    } | ConvertTo-Json

    $regRes = Invoke-RestMethod -Uri "$baseUrl/api/register" -Method POST -Body $studentPayload -ContentType "application/json"
    Write-Host "Response: $($regRes | ConvertTo-Json -Compress)" -ForegroundColor Green

    if ($regRes.status -ne "success") { throw "Registration status not success!" }
    if ($regRes.participant.reg_id -ne $uniqueId) { throw "Returned Reg ID does not match!" }
    if ($regRes.participant.event_name -ne "Quiz Competition") { throw "Returned event name mismatch!" }
    if (-not $regRes.participant.confirmation_no -or -not $regRes.participant.confirmation_no.StartsWith("CNF")) {
        throw "Returned confirmation number is invalid: $($regRes.participant.confirmation_no)"
    }
    Write-Host "-> Unique Confirmation Number generated: $($regRes.participant.confirmation_no)" -ForegroundColor Cyan
    Write-Host "-> PASS: Student successfully registered with unique confirmation details." -ForegroundColor Green

    # --- TEST 6: Duplicate Registration Attempt Flow ---
    Write-Host "`n[TEST 6] Testing Duplicate Registration Rejection with ID $uniqueId for Event 3..." -ForegroundColor Yellow
    $dupPayload = @{
        reg_id = $uniqueId.ToLower() # test case insensitivity
        name = "Kavitha S"
        department = "AI & DS"
        event_id = 3
    } | ConvertTo-Json

    $dupBlocked = $false
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/register" -Method POST -Body $dupPayload -ContentType "application/json"
    } catch {
        $dupBlocked = $true
        Write-Host "Caught expected rejection: $($_.Exception.Message)" -ForegroundColor Magenta
    }
    if (-not $dupBlocked) { throw "Duplicate was not blocked!" }
    Write-Host "-> PASS: Duplicate student ID rejected as expected." -ForegroundColor Green

    # --- TEST 7: Multi-Event Registration Using Different Registration ID ---
    Write-Host "`n[TEST 7] Registering same participant for second event with different Reg ID..." -ForegroundColor Yellow
    $uniqueId2 = "STU" + (Get-Random -Minimum 1000 -Maximum 9999)
    $secondEventPayload = @{
        reg_id = $uniqueId2
        name = "Kavitha Sundaram" # Same participant name
        department = "Artificial Intelligence and Data Science"
        event_id = 1 # Coding Competition
    } | ConvertTo-Json

    $regRes2 = Invoke-RestMethod -Uri "$baseUrl/api/register" -Method POST -Body $secondEventPayload -ContentType "application/json"
    if ($regRes2.status -ne "success" -or $regRes2.participant.reg_id -ne $uniqueId2) {
        throw "Failed to register participant for second event with different ID!"
    }
    Write-Host "-> PASS: Participant successfully registered for multiple events using distinct Registration IDs." -ForegroundColor Green

    # --- TEST 8: Verify Record in participants.txt ---
    Write-Host "`n[TEST 8] Verifying new student record in data/participants.txt..." -ForegroundColor Yellow
    $lastLine = Get-Content "data/participants.txt" | Select-Object -Last 1
    Write-Host "Last record in file: $lastLine" -ForegroundColor Cyan
    if (-not $lastLine.Contains($uniqueId2)) {
        throw "Record was not appended to data/participants.txt!"
    }
    Write-Host "-> PASS: Student record verified in persistent database." -ForegroundColor Green

    Write-Host "`n==========================================================" -ForegroundColor Green
    Write-Host "  STUDENT INTERFACE INTEGRATION TESTS ALL PASSED!        " -ForegroundColor Green
    Write-Host "==========================================================" -ForegroundColor Green

} finally {
    Write-Host "`nStopping server process (PID: $($proc.Id))..." -ForegroundColor Gray
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
}
