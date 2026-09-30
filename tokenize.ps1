# VIC-20 BASIC 2.0 tokenizer
# Reads a plain-text BASIC listing, writes a tokenized .prg

param(
    [Parameter(Mandatory=$true)][string]$InFile,
    [Parameter(Mandatory=$true)][string]$OutFile,
    [int]$LoadAddr = 0x0401
)

# ---- VIC-20 BASIC 2.0 token table ----
$tokens = @{
    'END'=0x80; 'FOR'=0x81; 'NEXT'=0x82; 'DATA'=0x83; 'INPUT#'=0x84
    'INPUT'=0x85; 'DIM'=0x86; 'READ'=0x87; 'LET'=0x88; 'GOTO'=0x89
    'RUN'=0x8A; 'IF'=0x8B; 'RESTORE'=0x8C; 'GOSUB'=0x8D; 'RETURN'=0x8E
    'REM'=0x8F; 'STOP'=0x90; 'ON'=0x91; 'WAIT'=0x92; 'LOAD'=0x93
    'SAVE'=0x94; 'VERIFY'=0x95; 'DEF'=0x96; 'POKE'=0x97; 'PRINT#'=0x98
    'PRINT'=0x99; 'CONT'=0x9A; 'LIST'=0x9B; 'CLR'=0x9C; 'CMD'=0x9D
    'SYS'=0x9E; 'OPEN'=0x9F; 'CLOSE'=0xA0; 'GET'=0xA1; 'NEW'=0xA2
    'TAB('=0xA3; 'TO'=0xA4; 'FN'=0xA5; 'SPC('=0xA6; 'THEN'=0xA7
    'NOT'=0xA8; 'STEP'=0xA9; '+'=0xAA; '-'=0xAB; '*'=0xAC
    '/'=0xAD; '^'=0xAE; 'AND'=0xAF; 'OR'=0xB0; '>'=0xB1
    '='=0xB2; '<'=0xB3; 'SGN'=0xB4; 'INT'=0xB5; 'ABS'=0xB6
    'USR'=0xB7; 'FRE'=0xB8; 'POS'=0xB9; 'SQR'=0xBA; 'RND'=0xBB
    'LOG'=0xBC; 'EXP'=0xBD; 'COS'=0xBE; 'SIN'=0xBF; 'TAN'=0xC0
    'ATN'=0xC1; 'PEEK'=0xC2; 'LEN'=0xC3; 'STR$'=0xC4; 'VAL'=0xC5
    'ASC'=0xC6; 'CHR$'=0xC7; 'LEFT$'=0xC8; 'RIGHT$'=0xC9; 'MID$'=0xCA
    'GO'=0xCB
}

# Sort by keyword length descending so longer keywords match first
# (e.g. PRINT# before PRINT, LEFT$ before LEFT).
$tokenList = @($tokens.GetEnumerator() | Sort-Object { $_.Key.Length } -Descending)

# ---- Read the source file ----
$srcPath = [System.IO.Path]::Combine((Get-Location).Path, $InFile)
if (-not (Test-Path -LiteralPath $srcPath)) {
    Write-Error "Input file not found: $srcPath"
    exit 1
}
$lines = Get-Content -LiteralPath $srcPath
Write-Host "Read $($lines.Count) lines from $srcPath"

# ---- Parse into (lineno, body) pairs ----
$parsed = New-Object System.Collections.Generic.List[object]
foreach ($line in $lines) {
    $t = $line.TrimEnd()
    if ($t -match '^\s*(\d+)\s+(.*)$') {
        $lineno = [int]$Matches[1]
        $body   = $Matches[2]
        $parsed.Add([pscustomobject]@{ Line = $lineno; Body = $body })
    } elseif ($t -match '^\s*(\d+)\s*$') {
        $parsed.Add([pscustomobject]@{ Line = [int]$Matches[1]; Body = '' })
    } else {
        if ($t.Length -gt 0) {
            Write-Warning "Skipping line without number: $t"
        }
    }
}
Write-Host "Parsed $($parsed.Count) BASIC lines"

# ---- Tokenize one line body ----
function Tokenize-Body([string]$body) {
    $out = New-Object System.Collections.Generic.List[byte]
    $i = 0
    $n = $body.Length
    $upper = $body.ToUpperInvariant()

    $inString = $false
    $inRem = $false

    while ($i -lt $n) {
        $c = $upper[$i]

        if ($inRem) {
            # Copy the rest verbatim
            $out.Add([byte][char]$upper[$i])
            $i++
            continue
        }

        if ($inString) {
            $out.Add([byte][char]$upper[$i])
            if ($upper[$i] -eq '"') { $inString = $false }
            $i++
            continue
        }

        if ($c -eq '"') {
            $out.Add([byte][char]$c)
            $inString = $true
            $i++
            continue
        }

        # Try to match a keyword at position i
        $matched = $false
        foreach ($kv in $tokenList) {
            $kw = $kv.Key
            if ($kw.Length -le ($n - $i)) {
                if ($upper.Substring($i, $kw.Length) -ceq $kw) {
                    $out.Add([byte]$kv.Value)
                    $i += $kw.Length
                    if ($kw -eq 'REM') { $inRem = $true }
                    $matched = $true
                    break
                }
            }
        }

        if (-not $matched) {
            $out.Add([byte][char]$c)
            $i++
        }
    }

    return $out.ToArray()
}

# ---- Tokenize all lines ----
$tokenized = New-Object System.Collections.Generic.List[object]
foreach ($p in $parsed) {
    $bytes = Tokenize-Body $p.Body
    $tokenized.Add([pscustomobject]@{ Line = $p.Line; Bytes = $bytes })
}

# ---- Compute line start addresses ----
$lineStarts = New-Object System.Collections.Generic.List[int]
$addr = $LoadAddr
foreach ($t in $tokenized) {
    $lineStarts.Add($addr)
    # link (2) + lineno (2) + body + terminator (1)
    $addr += 2 + 2 + $t.Bytes.Length + 1
}
$endAddr = $addr

# ---- Assemble PRG bytes ----
$mem = New-Object System.Collections.Generic.List[byte]

# Load address header (little-endian)
$mem.Add([byte]($LoadAddr -band 0xFF))
$mem.Add([byte](($LoadAddr -shr 8) -band 0xFF))

for ($idx = 0; $idx -lt $tokenized.Count; $idx++) {
    $t = $tokenized[$idx]
    $lineno = $t.Line
    $body = $t.Bytes

    if ($idx -lt $tokenized.Count - 1) {
        $link = $lineStarts[$idx + 1]
    } else {
        $link = $endAddr
    }

    $mem.Add([byte]($link -band 0xFF))
    $mem.Add([byte](($link -shr 8) -band 0xFF))
    $mem.Add([byte]($lineno -band 0xFF))
    $mem.Add([byte](($lineno -shr 8) -band 0xFF))

    foreach ($b in $body) { $mem.Add($b) }

    $mem.Add([byte]0)   # end-of-line marker
}

# Program terminator
$mem.Add([byte]0)
$mem.Add([byte]0)

# ---- Write output ----
$outPath = [System.IO.Path]::Combine((Get-Location).Path, $OutFile)
[System.IO.File]::WriteAllBytes($outPath, $mem.ToArray())

Write-Host "Wrote $($mem.Count) bytes to $outPath"
Write-Host ("First line starts at {0:X4}" -f $lineStarts[0])
Write-Host ("Program ends at {0:X4}" -f $endAddr)