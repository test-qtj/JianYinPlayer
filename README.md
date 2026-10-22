# JianYinPlayer

A local music player built with Qt Widgets. The repository name is JianYinPlayer; the original project and application files use the name QQMusic.

## Features

- Local music library and playback
- Recently played and favorites views
- Full-screen lyrics view with the current track's cover art
- Import local audio files
- Batch import local NCM files through an external converter

## Build on Windows

1. Install Qt Creator with a Qt 5.14 MinGW 64-bit kit. The project uses Qt Core, Gui, Widgets, Multimedia, and SQL.
2. Open QQMusic/QQMusic.pro in Qt Creator.
3. Select the Desktop Qt 5.14 MinGW 64-bit kit and configure the project.
4. Build and run from Qt Creator.

The repository contains source code and UI assets. It does not include a prebuilt executable or personal music files.

## Import NCM files

The NCM import option calls the separately installed [NCM2MP3](https://github.com/charlotte-xiao/NCM2MP3) command-line JAR. Java 8 or newer and the converter JAR are required; neither is bundled here. In the player, use the + button, choose 导入 NCM 音乐, then select files or scan a folder. The first use may ask you to locate the JAR.

Converted audio is copied to Music/JianYinPlayer/转换音乐 and added to the local library. Original NCM files are left in place. Same-name LRC lyric files beside the source NCM files are copied when present; the converter does not fetch lyrics.

## Repository layout

- QQMusic/ — Qt project sources, forms, and bundled UI resources
