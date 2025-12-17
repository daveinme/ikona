$BuildDir = "C:\Users\Dave\Desktop\codess\ikona\build"
$SourceDir = "C:\Users\Dave\Desktop\codess\ikona"
$GtkDir = "C:\msys64\mingw64"
$OutputDir = "C:\Users\Dave\Desktop\ikona-portable"

Write-Host "IKONA PORTABLE BUILDER"
Write-Host "====================="
Write-Host ""

if (Test-Path $OutputDir) {
    Remove-Item -Path $OutputDir -Recurse -Force
}
New-Item -ItemType Directory -Path $OutputDir | Out-Null

Copy-Item -Path "$BuildDir\ikona.exe" -Destination "$OutputDir\ikona.exe" -Force
Copy-Item -Path "$SourceDir\style.css" -Destination "$OutputDir\style.css" -Force

$dllDir = "$GtkDir\bin"
$dlls = @(
    # Core GTK+3
    "libgtk-3-0.dll",
    "libgdk-3-0.dll",
    "libglib-2.0-0.dll",
    "libgobject-2.0-0.dll",
    "libgio-2.0-0.dll",
    "libgdk_pixbuf-2.0-0.dll",

    # Rendering
    "libcairo-2.dll",
    "libpango-1.0-0.dll",
    "libpangocairo-1.0-0.dll",
    "libpangowin32-1.0-0.dll",
    "libatk-1.0-0.dll",
    "libepoxy-0.dll",

    # Fonts & Text
    "libharfbuzz-0.dll",
    "libfreetype-6.dll",
    "libfontconfig-1.dll",
    "libfribidi-0.dll",
    "libgraphite2.dll",

    # Images
    "libpng16-16.dll",
    "libjpeg-8.dll",
    "libtiff-6.dll",

    # Compression
    "zlib1.dll",
    "libbz2-1.dll",
    "liblzma-5.dll",
    "libdeflate.dll",

    # Brotli
    "libbrotlidec.dll",
    "libbrotlicommon.dll",

    # Utils
    "libintl-8.dll",
    "libiconv-2.dll",
    "libwinpthread-1.dll",
    "libffi-8.dll",
    "libpcre2-8-0.dll",
    "libexpat-1.dll",

    # Cairo dependencies
    "libpixman-1-0.dll"
)

$count = 0
$missing = @()
foreach ($dll in $dlls) {
    $src = "$dllDir\$dll"
    if (Test-Path $src) {
        Copy-Item -Path $src -Destination "$OutputDir\$dll" -Force
        $count++
    } else {
        $missing += $dll
    }
}

Write-Host "Copying GTK resources..."
$libDir = "$GtkDir\lib"
$shareDir = "$GtkDir\share"

# GDK-Pixbuf loaders (critical for loading images)
if (Test-Path "$libDir\gdk-pixbuf-2.0") {
    Copy-Item -Path "$libDir\gdk-pixbuf-2.0" -Destination "$OutputDir\lib\gdk-pixbuf-2.0" -Recurse -Force
    Write-Host "  Copied gdk-pixbuf loaders"
}

# GTK themes and icons
if (Test-Path "$shareDir\icons") {
    New-Item -ItemType Directory -Path "$OutputDir\share" -Force | Out-Null
    Copy-Item -Path "$shareDir\icons\Adwaita" -Destination "$OutputDir\share\icons\Adwaita" -Recurse -Force -ErrorAction SilentlyContinue
    Copy-Item -Path "$shareDir\icons\hicolor" -Destination "$OutputDir\share\icons\hicolor" -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "  Copied icon themes"
}

# GLib schemas
if (Test-Path "$shareDir\glib-2.0\schemas") {
    Copy-Item -Path "$shareDir\glib-2.0" -Destination "$OutputDir\share\glib-2.0" -Recurse -Force
    Write-Host "  Copied GLib schemas"
}

$batch = "@echo off`ncd /d `"%~dp0`"`nset PATH=%~dp0;%PATH%`nset GDK_PIXBUF_MODULEDIR=%~dp0lib\gdk-pixbuf-2.0\2.10.0\loaders`nset GDK_PIXBUF_MODULE_FILE=%~dp0lib\gdk-pixbuf-2.0\2.10.0\loaders.cache`nstart `"`" ikona.exe"
$batch | Out-File -FilePath "$OutputDir\launch.bat" -Encoding ASCII

Write-Host ""
Write-Host "====================================="
Write-Host "Created portable version at:"
Write-Host "$OutputDir"
Write-Host ""
Write-Host "Copied $count DLL files"
if ($missing.Count -gt 0) {
    Write-Host ""
    Write-Host "WARNING: Missing DLLs:" -ForegroundColor Yellow
    foreach ($m in $missing) {
        Write-Host "  - $m" -ForegroundColor Yellow
    }
}
Write-Host ""
Write-Host "Created launch.bat"
Write-Host ""
Write-Host "To run: double-click launch.bat"
Write-Host "====================================="
