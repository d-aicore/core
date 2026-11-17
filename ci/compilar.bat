@echo off
rem Core - compilar com GUI. Duplo clique neste ficheiro.
rem Escolhe a pasta de build (fora do repo), o tipo e os jobs, e carrega em Config+Compilar.
rem O botao Gate corre ci\gate.ps1: Release + TESTES=ON + ctest -L merge --no-tests=error.
rem Nao instala nada e nao escreve na arvore do codigo. O build vai para a pasta que escolheres.
powershell -NoProfile -ExecutionPolicy Bypass -Command "$self='%~f0'; $s=Get-Content -LiteralPath $self -Raw; $m=[regex]::Match($s,'(?m)^#PS1#\r?$'); if(-not $m.Success){exit 2}; iex $s.Substring($m.Index+$m.Length)"
exit /b %errorlevel%
#PS1#
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()

$batDir = Split-Path -Parent $self
$cfgDir = Join-Path $(if ($env:LOCALAPPDATA) { $env:LOCALAPPDATA } else { $batDir }) 'CoreCompilar'
$cfgPath = Join-Path $cfgDir 'escolhas.json'
# Uma excecao nao tratada numa thread do threadpool mata o processo sem janela nenhuma: fica registada.
$global:crashLog = Join-Path $cfgDir 'compilar-crash.txt'
[AppDomain]::CurrentDomain.add_UnhandledException({ param($s, $e) try { [IO.File]::AppendAllText($global:crashLog, (Get-Date -Format 's') + ' ' + $e.ExceptionObject.ToString() + "`r`n") } catch { } })
[Windows.Forms.Application]::add_ThreadException({ param($s, $e) try { [IO.File]::AppendAllText($global:crashLog, (Get-Date -Format 's') + ' UI: ' + $e.Exception.ToString() + "`r`n") } catch { } })
$cfg = $null
if (Test-Path -LiteralPath $cfgPath) { try { $cfg = Get-Content -LiteralPath $cfgPath -Raw | ConvertFrom-Json } catch { $cfg = $null } }
function Cfg($n, $d) { if ($cfg -and ($cfg.PSObject.Properties.Name -contains $n)) { $cfg.$n } else { $d } }

$repoGuess = ''
foreach ($c in @($batDir, (Split-Path -Parent $batDir))) {
  if ($c -and (Test-Path -LiteralPath (Join-Path $c 'CMakeLists.txt'))) { $repoGuess = $c; break }
}
if (-not $repoGuess -and (Test-Path -LiteralPath 'D:\CORE\CORE\CMakeLists.txt')) { $repoGuess = 'D:\CORE\CORE' }
$juceGuess = ''
if (Test-Path -LiteralPath 'D:\Core-deps\JUCE-9.0.5') { $juceGuess = 'D:\Core-deps\JUCE-9.0.5' }

$f = New-Object Windows.Forms.Form
$f.Text = 'Core - compilar'
$f.ClientSize = New-Object Drawing.Size(904, 600)
$f.StartPosition = 'CenterScreen'
$f.AutoScaleMode = 'None'
$f.MinimumSize = $f.Size
$tip = New-Object Windows.Forms.ToolTip

function Lbl($t, $x, $y, $w) {
  $l = New-Object Windows.Forms.Label; $l.Text = $t; $l.Location = New-Object Drawing.Point($x, $y)
  $l.Size = New-Object Drawing.Size($w, 20); $f.Controls.Add($l); $l
}
function Txt($x, $y, $w, $v) {
  $t = New-Object Windows.Forms.TextBox; $t.Location = New-Object Drawing.Point($x, $y)
  $t.Size = New-Object Drawing.Size($w, 23); $t.Text = $v; $f.Controls.Add($t); $t
}
function Btn($t, $x, $y, $w) {
  $b = New-Object Windows.Forms.Button; $b.Text = $t; $b.Location = New-Object Drawing.Point($x, $y)
  $b.Size = New-Object Drawing.Size($w, 30); $f.Controls.Add($b); $b
}
function Chk($t, $x, $y, $w, $on) {
  $c = New-Object Windows.Forms.CheckBox; $c.Text = $t; $c.Location = New-Object Drawing.Point($x, $y)
  $c.Size = New-Object Drawing.Size($w, 22); $c.Checked = $on; $f.Controls.Add($c); $c
}
function Combo($x, $y, $w, $items, $v, $editable) {
  $c = New-Object Windows.Forms.ComboBox; $c.Location = New-Object Drawing.Point($x, $y)
  $c.Size = New-Object Drawing.Size($w, 23); $c.DropDownStyle = $(if ($editable) { 'DropDown' } else { 'DropDownList' })
  foreach ($i in $items) { [void]$c.Items.Add($i) }
  $c.Text = $v; $f.Controls.Add($c); $c
}

Lbl 'Repo' 12 17 64 | Out-Null
$repo = Txt 84 14 752 (Cfg 'repo' $repoGuess)
$repoBtn = Btn '...' 842 13 48
Lbl 'Build' 12 49 64 | Out-Null
$build = Combo 84 46 752 @() (Cfg 'build' 'D:\Core-build\gui') $true
$buildBtn = Btn '...' 842 45 48
Lbl 'JUCE' 12 81 64 | Out-Null
$juce = Txt 84 78 640 (Cfg 'juce' $juceGuess)
Lbl '(vazio = descarrega)' 730 81 110 | Out-Null
$juceBtn = Btn '...' 842 77 48

Lbl 'Tipo' 12 115 40 | Out-Null
$type = Combo 56 112 90 @('Release', 'Debug') (Cfg 'type' 'Release') $false
if ($type.SelectedIndex -lt 0) { $type.SelectedIndex = 0 }
Lbl 'Jobs' 160 115 32 | Out-Null
$jobs = New-Object Windows.Forms.NumericUpDown
$jobs.Location = New-Object Drawing.Point(196, 112); $jobs.Size = New-Object Drawing.Size(56, 23)
$jobs.Minimum = 1; $jobs.Maximum = 64
$jobs.Value = [Math]::Max(1, [Math]::Min(64, [int](Cfg 'jobs' 8)))
$f.Controls.Add($jobs)
$app = Chk 'APP' 268 114 64 ([bool](Cfg 'app' $true))
$tests = Chk 'TESTES' 336 114 76 ([bool](Cfg 'tests' $true))
$gateApp = Chk 'Gate tambem com a app' 418 114 170 ([bool](Cfg 'gateApp' $false))

$bx = 12
$bCfg = Btn 'Configurar' $bx 146 110; $bx += 116
$bBuild = Btn 'Compilar' $bx 146 110; $bx += 116
$bBoth = Btn 'Config+Compilar' $bx 146 145; $bx += 151
$bGate = Btn 'Gate (merge)' $bx 146 120; $bx += 126
$stop = Btn 'Parar' $bx 146 90; $bx += 96
$openDir = Btn 'Abrir pasta' $bx 146 110; $bx += 116
$openExe = Btn 'Abrir Core.exe' $bx 146 130

$log = New-Object Windows.Forms.TextBox
$log.Location = New-Object Drawing.Point(12, 186); $log.Size = New-Object Drawing.Size(878, 372)
$log.Multiline = $true; $log.ReadOnly = $true; $log.ScrollBars = 'Both'; $log.WordWrap = $false
$log.Font = New-Object Drawing.Font('Consolas', 9)
$log.BackColor = [Drawing.Color]::FromArgb(24, 24, 24); $log.ForeColor = [Drawing.Color]::FromArgb(232, 232, 232)
$log.Anchor = 'Top,Left,Right,Bottom'
$f.Controls.Add($log)
$status = Lbl 'pronto' 12 570 700
$status.Anchor = 'Bottom,Left'
$progress = New-Object Windows.Forms.ProgressBar
$progress.Location = New-Object Drawing.Point(724, 566); $progress.Size = New-Object Drawing.Size(166, 20)
$progress.Anchor = 'Bottom,Right'
$f.Controls.Add($progress)

$tip.SetToolTip($repo, 'Pasta com CMakeLists.txt. Detectada a partir da localizacao deste .bat.')
$tip.SetToolTip($build, 'Pasta de destino, fora do repo. Escreve um caminho novo ou escolhe um build ja existente. E criada se nao existir.')
$tip.SetToolTip($juce, 'Copia local do JUCE 9.0.5. Vazio = o CMake descarrega a versao fixada no CMakeLists.txt.')
$tip.SetToolTip($jobs, 'Compiladores em paralelo. 8 e o que o ci/gate.ps1 usa nesta maquina (8 nucleos, 16 threads, 31 GiB).')
$tip.SetToolTip($type, 'Ninja e single-config: mudar o tipo reconfigura a pasta de build.')
$tip.SetToolTip($gateApp, 'Acrescenta -Desktop ao gate: o self-test da app, que abre a janela do Core durante ~20 s.')
$tip.SetToolTip($bGate, 'ci/gate.ps1: configura Release + TESTES=ON, compila e corre ctest -L merge --no-tests=error --timeout 600. A escolha de Tipo acima nao se aplica ao gate.')
$tip.SetToolTip($stop, 'Para a tarefa em curso (taskkill /T /F a arvore de processos deste build).')

# Estado da tarefa em $global: os eventos do processo correm em threads do threadpool, onde o scope
# filho do iex nao e visivel. Sem eventos nenhuns: o runner escreve a saida num ficheiro e um Timer
# no thread da UI le esse ficheiro e o HasExited. Medido: com eventos + scriptblocks o processo morria.
$global:proc = $null
$global:reader = $null
$global:saida = ''
$global:running = $false
$global:exitCode = 0
$global:sw = $null
$global:title = ''
$global:runBuild = ''

function Log($t) { if (-not $log.IsDisposed) { $log.AppendText($t + "`r`n"); $log.ScrollToCaret() } }
function Full($p) { [IO.Path]::GetFullPath($p) }
function Inside($child, $parent) {
  $c = (Full $child).TrimEnd('\') + '\'
  $p = (Full $parent).TrimEnd('\') + '\'
  $c.StartsWith($p, [StringComparison]::OrdinalIgnoreCase)
}
function CacheValue($file, $key) {
  $l = Get-Content -LiteralPath $file | Where-Object { $_ -like ($key + ':*') } | Select-Object -First 1
  if ($l -and $l.Contains('=')) { $l.Substring($l.IndexOf('=') + 1) } else { '' }
}
function Norm($p) { (Full $p).Replace('/', '\').TrimEnd('\') }

function Vcvars {
  # O VS 2022 desta maquina nao tem vcvars64.bat; o VS 18 BuildTools tem (licao do ci/gate.ps1).
  $known = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
  if (Test-Path -LiteralPath $known) { return $known }
  if (${env:ProgramFiles(x86)}) {
    $vw = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vw) {
      $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
      $all = & $vw -all -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
      $ErrorActionPreference = $old
      foreach ($i in @($all)) {
        if ($i) { $p = Join-Path $i 'VC\Auxiliary\Build\vcvars64.bat'; if (Test-Path -LiteralPath $p) { return $p } }
      }
    }
  }
  return $null
}

function GitInfo($dir) {
  $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
  $sha = & git -C $dir rev-parse --short HEAD 2>$null
  $dirty = & git -C $dir status --porcelain 2>$null
  $ErrorActionPreference = $old
  if ($sha) { "$sha" + $(if ($dirty) { ' (com alteracoes por commitar)' } else { '' }) } else { 'sem git' }
}

function ExePath {
  $b = $build.Text.Trim()
  if (-not $b) { return '' }
  Join-Path $b ('app\desktop\CoreApp_artefacts\' + $type.Text + '\Core.exe')
}
function RefreshExe {
  $p = ExePath
  $ok = ($p -and (Test-Path -LiteralPath $p))
  $openExe.Enabled = $ok
  $tip.SetToolTip($openExe, $(if ($ok) { $p } else { 'Ainda nao existe: ' + $p }))
}

function SaveCfg {
  $h = @($build.Text.Trim())
  foreach ($i in $build.Items) { if ($i -and ($h -notcontains $i)) { $h += $i } }
  $h = @($h | Where-Object { $_ } | Select-Object -First 12)
  $o = [pscustomobject]@{
    repo = $repo.Text.Trim(); build = $build.Text.Trim(); juce = $juce.Text.Trim(); type = $type.Text
    jobs = [int]$jobs.Value; app = $app.Checked; tests = $tests.Checked; gateApp = $gateApp.Checked; history = $h
  }
  # Memorizar e melhor-esforco: uma falha aqui nao pode estragar um build.
  try {
    New-Item -ItemType Directory -Force -Path $cfgDir | Out-Null
    $o | ConvertTo-Json | Set-Content -LiteralPath $cfgPath -Encoding UTF8
  } catch { }
}

function CfgLine {
  $a = 'cmake -S "' + $repo.Text.Trim() + '" -B "' + $build.Text.Trim() + '" -G Ninja -DCMAKE_BUILD_TYPE=' + $type.Text
  $a += ' -DCORE_BUILD_APP=' + $(if ($app.Checked) { 'ON' } else { 'OFF' })
  $a += ' -DCORE_BUILD_TESTS=' + $(if ($tests.Checked) { 'ON' } else { 'OFF' })
  $a += ' -DCMAKE_EXPORT_COMPILE_COMMANDS=ON'
  $j = $juce.Text.Trim()
  # Audit 31: o -D com um caminho vai TODO entre aspas, senao um caminho com espacos parte-se em dois argumentos.
  if ($j) { $a += ' "-DFETCHCONTENT_SOURCE_DIR_JUCE=' + $j + '"' }
  $a
}
function BuildLine { 'cmake --build "' + $build.Text.Trim() + '" --parallel ' + [int]$jobs.Value }

function SetBusy([bool]$on) {
  foreach ($x in @($bCfg, $bBuild, $bBoth, $bGate, $repoBtn, $buildBtn, $juceBtn, $repo, $build, $juce, $type, $jobs, $app, $tests, $gateApp)) { $x.Enabled = (-not $on) }
  $stop.Enabled = $on
  $progress.Style = $(if ($on) { 'Marquee' } else { 'Blocks' })
  if (-not $on) { $progress.Value = 0 }
  RefreshExe
}

function Finish {
  $global:running = $false
  if ($global:reader) { $global:reader.Dispose(); $global:reader = $null }
  $secs = [int]$global:sw.Elapsed.TotalSeconds
  $code = $global:exitCode
  Log ''
  Log ('== exit ' + $code + '  (' + $secs + ' s) ==')
  $lp = Join-Path $global:runBuild 'compilar.log'
  try { [IO.File]::WriteAllText($lp, $log.Text, (New-Object Text.UTF8Encoding $false)); Log ('log: ' + $lp) }
  catch { Log ('aviso: nao consegui gravar o log em ' + $lp + ': ' + $_.Exception.Message) }
  $status.Text = $global:title + ': exit ' + $code + '  (' + $secs + ' s)' + $(if ($code -ne 0) { '  VERMELHO' } else { '  VERDE' })
  SetBusy $false
  SaveCfg
  $timer.Stop()
}

function Start-Run([string]$title, [string[]]$cmds, [switch]$SkipVcvars) {
  if ($global:running) { Log 'ERRO: ja ha uma tarefa a correr. Espera que acabe ou carrega em Parar.'; return }
  $r = $repo.Text.Trim(); $b = $build.Text.Trim(); $j = $juce.Text.Trim()
  if (-not $r -or -not (Test-Path -LiteralPath (Join-Path $r 'CMakeLists.txt'))) { Log ('ERRO: repo invalido, falta CMakeLists.txt em: ' + $r); return }
  if (-not $b) { Log 'ERRO: escolhe a pasta de build.'; return }
  if (Inside $b $r) { Log ('ERRO: a pasta de build esta dentro do repo (' + $b + '). Escolhe fora, p.ex. D:\Core-build\gui. O .gitignore so cobre build/ e build-*/.'); return }
  if ($j -and -not (Test-Path -LiteralPath $j)) { Log ('ERRO: JUCE local nao existe: ' + $j + '. Deixa o campo vazio para o CMake descarregar a versao fixada.'); return }
  $cache = Join-Path $b 'CMakeCache.txt'
  if (Test-Path -LiteralPath $cache) {
    $h = CacheValue $cache 'CMAKE_HOME_DIRECTORY'
    if ($h -and ((Norm $h) -ne (Norm $r))) { Log ('ERRO: essa pasta ja foi configurada para outro repo (' + $h + '). Escolhe outra pasta; nada aqui e apagado.'); return }
    $g = CacheValue $cache 'CMAKE_GENERATOR'
    if ($g -and $g -ne 'Ninja') { Log ('ERRO: essa pasta foi gerada com ' + $g + '; este launcher usa Ninja. Escolhe outra pasta.'); return }
  }
  $vc = $null
  if (-not $SkipVcvars) {
    $vc = Vcvars
    if (-not $vc) { Log 'ERRO: vcvars64.bat nao encontrado. Instala o VS BuildTools com a carga de trabalho C++.'; return }
  }
  New-Item -ItemType Directory -Force -Path $b | Out-Null
  $bat = Join-Path $b 'compilar-run.bat'
  $lines = @('@echo off', 'chcp 65001 >nul')
  if ($vc) {
    $lines += 'call "' + $vc + '" > "' + (Join-Path $b 'vcvars.log') + '" 2>&1'
    $lines += 'if errorlevel 1 exit /b 1'
  }
  foreach ($c in $cmds) { $lines += $c; $lines += 'if errorlevel 1 exit /b 1' }
  $lines += 'exit /b 0'
  [IO.File]::WriteAllText($bat, ($lines -join "`r`n") + "`r`n", (New-Object Text.UTF8Encoding $false))

  $log.Clear()
  $global:title = $title; $global:runBuild = $b
  Log ('== ' + $title + ' ==  ' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))
  Log ('repo  : ' + $r + ' @ ' + (GitInfo $r))
  Log ('build : ' + $b + '  (' + $type.Text + ', jobs ' + [int]$jobs.Value + ', APP=' + $(if ($app.Checked) { 'ON' } else { 'OFF' }) + ', TESTS=' + $(if ($tests.Checked) { 'ON' } else { 'OFF' }) + ')')
  if ($j) { Log ('JUCE  : ' + $j + ' (copia local)') } else { Log 'JUCE  : download da versao fixada no CMakeLists.txt' }
  if ($vc) { Log ('vcvars: ' + $vc + '  (saida em ' + (Join-Path $b 'vcvars.log') + ')') }
  Log ('runner: ' + $bat)
  foreach ($c in $cmds) { Log ('> ' + $c) }
  Log ''
  SaveCfg

  $global:saida = Join-Path $b 'compilar-saida.log'
  Remove-Item -LiteralPath $global:saida -Force -ErrorAction SilentlyContinue
  $psi = New-Object Diagnostics.ProcessStartInfo
  $psi.FileName = Join-Path $env:SystemRoot 'System32\cmd.exe'
  $psi.Arguments = '/c ""' + $bat + '" > "' + $global:saida + '" 2>&1"'
  $psi.WorkingDirectory = $r
  $psi.UseShellExecute = $false
  $psi.CreateNoWindow = $true
  # Lancado de um shell POSIX (Git Bash) o TEMP vem como /tmp e o cl.exe parte; o filho recebe sempre um TEMP Windows.
  $tmp = $psi.EnvironmentVariables['TEMP']
  if (-not $tmp -or -not (Test-Path -LiteralPath $tmp)) { $psi.EnvironmentVariables['TEMP'] = $b; $psi.EnvironmentVariables['TMP'] = $b }
  $p = New-Object Diagnostics.Process
  $p.StartInfo = $psi
  $global:proc = $p
  $global:reader = $null
  $global:running = $true; $global:exitCode = 0
  $global:sw = [Diagnostics.Stopwatch]::StartNew()
  SetBusy $true
  $status.Text = $title + ' a correr...'
  $timer.Start()
  try { [void]$p.Start() }
  catch {
    Log ('ERRO a arrancar a tarefa: ' + $_.Exception.Message)
    $global:running = $false; $timer.Stop(); SetBusy $false
    $status.Text = 'erro a arrancar'
  }
}

$timer = New-Object Windows.Forms.Timer
$timer.Interval = 120
$timer.Add_Tick({
  if ($f.IsDisposed) { $timer.Stop(); return }
  if (-not $global:reader -and $global:saida -and (Test-Path -LiteralPath $global:saida)) {
    $fs = [IO.File]::Open($global:saida, 'Open', 'Read', 'ReadWrite')
    $global:reader = New-Object IO.StreamReader($fs, [Text.Encoding]::UTF8)
  }
  if ($global:reader) {
    $t = $global:reader.ReadToEnd()
    if ($t) { $log.AppendText($t); $log.ScrollToCaret() }
  }
  if ($global:running) {
    $status.Text = $global:title + ' a correr... ' + [int]$global:sw.Elapsed.TotalSeconds + ' s'
    if ($global:proc.HasExited) {
      $global:exitCode = $global:proc.ExitCode
      if ($global:reader) { $t = $global:reader.ReadToEnd(); if ($t) { $log.AppendText($t); $log.ScrollToCaret() } }
      Finish
    }
  }
})

function Pick($tb, $title) {
  $d = New-Object Windows.Forms.FolderBrowserDialog
  $d.Description = $title; $d.ShowNewFolderButton = $true
  $cur = $tb.Text.Trim()
  if ($cur -and (Test-Path -LiteralPath $cur)) { $d.SelectedPath = $cur }
  elseif ($cur) { $par = Split-Path -Parent $cur; if ($par -and (Test-Path -LiteralPath $par)) { $d.SelectedPath = $par } }
  if ($d.ShowDialog() -eq 'OK') { $tb.Text = $d.SelectedPath }
}

$repoBtn.Add_Click({ Pick $repo 'Escolhe a pasta do repo (a que tem CMakeLists.txt)' })
$buildBtn.Add_Click({ Pick $build 'Escolhe a pasta de build (fora do repo)' })
$juceBtn.Add_Click({ Pick $juce 'Escolhe a copia local do JUCE 9.0.5' })
$bCfg.Add_Click({ Start-Run 'Configurar' @((CfgLine)) })
$bBuild.Add_Click({ Start-Run 'Compilar' @((BuildLine)) })
$bBoth.Add_Click({ Start-Run 'Config+Compilar' @((CfgLine), (BuildLine)) })
$bGate.Add_Click({
  $g = Join-Path $repo.Text.Trim() 'ci\gate.ps1'
  if (-not (Test-Path -LiteralPath $g)) { Log ('ERRO: nao encontrei ' + $g); return }
  $c = 'powershell -NoProfile -ExecutionPolicy Bypass -File "' + $g + '" -Build "' + $build.Text.Trim() + '" -Jobs ' + [int]$jobs.Value
  $j = $juce.Text.Trim()
  if ($j) { $c += ' -Juce "' + $j + '"' }
  if ($gateApp.Checked) { $c += ' -Desktop' }
  Start-Run 'Gate (merge)' @($c) -SkipVcvars
  Log 'nota: o gate configura Release + TESTES=ON nesta pasta; a escolha de Tipo acima nao se aplica ao gate.'
})
$stop.Add_Click({
  if ($global:proc -and -not $global:proc.HasExited) {
    $procId = $global:proc.Id
    Log ('a parar: taskkill /PID ' + $procId + ' /T /F')
    $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    $out = & taskkill /PID $procId /T /F 2>&1
    $ErrorActionPreference = $old
    foreach ($l in @($out)) { Log ([string]$l) }
  }
})
$openDir.Add_Click({
  $b = $build.Text.Trim()
  if ($b -and (Test-Path -LiteralPath $b)) { Start-Process explorer.exe -ArgumentList ('"' + $b + '"') }
  else { Log ('ERRO: a pasta ainda nao existe: ' + $b) }
})
$openExe.Add_Click({
  $p = ExePath
  if ($p -and (Test-Path -LiteralPath $p)) { Start-Process -FilePath $p -WorkingDirectory (Split-Path -Parent $p) }
  else { Log ('ERRO: nao existe ' + $p) }
})
$build.Add_TextChanged({ RefreshExe })
$type.Add_SelectedIndexChanged({ RefreshExe })
$f.Add_FormClosing({
  param($s, $e)
  if ($global:running) {
    $r = [Windows.Forms.MessageBox]::Show('Ha uma tarefa a correr. Fechar e parar a tarefa?', 'Core - compilar', 'YesNo', 'Question')
    if ($r -ne 'Yes') { $e.Cancel = $true; return }
    $timer.Stop()
    if ($global:proc -and -not $global:proc.HasExited) {
      $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
      & taskkill /PID $global:proc.Id /T /F 2>&1 | Out-Null
      $ErrorActionPreference = $old
    }
  }
  SaveCfg
})

# Builds ja configurados debaixo de D:\Core-build entram na lista, para nao ser preciso decorar caminhos.
$hist = @(Cfg 'history' @())
$root = 'D:\Core-build'
if (Test-Path -LiteralPath $root) {
  foreach ($d in @(Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue)) {
    if (Test-Path -LiteralPath (Join-Path $d.FullName 'CMakeCache.txt')) { $hist += $d.FullName }
  }
}
foreach ($i in @($hist | Where-Object { $_ } | Select-Object -Unique)) { [void]$build.Items.Add($i) }
$build.Text = Cfg 'build' 'D:\Core-build\gui'

$stop.Enabled = $false
RefreshExe
[void]$f.ShowDialog()