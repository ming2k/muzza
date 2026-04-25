#!/bin/bash
set -e

OUTDIR="${1:-test_fixtures/generated}"
mkdir -p "$OUTDIR"

echo "Generating test fixtures in $OUTDIR..."

ffmpeg -f lavfi -i testsrc2=size=640x360:rate=30 -t 5 -an -y "$OUTDIR/video_only_5s.mp4" || true
ffmpeg -f lavfi -i sine=frequency=440:sample_rate=48000 -t 5 -ac 2 -y "$OUTDIR/audio_only_5s.wav" || true
ffmpeg -f lavfi -i testsrc2=size=640x360:rate=30 -f lavfi -i sine=frequency=440:sample_rate=48000 -t 10 -c:v libx264 -c:a aac -y "$OUTDIR/av_10s.mp4" || true
ffmpeg -f lavfi -i testsrc2=size=640x360:rate=30 -f lavfi -i sine=frequency=880:sample_rate=44100 -t 5 -c:v libx264 -c:a aac -y "$OUTDIR/av_44100hz.mp4" || true
ffmpeg -f lavfi -i testsrc2=size=320x180:rate=30 -f lavfi -i sine=frequency=330:sample_rate=48000 -t 1 -c:v libx264 -c:a aac -y "$OUTDIR/short_1s.mp4" || true
ffmpeg -f lavfi -i color=c=blue:s=320x180 -frames:v 1 -y "$OUTDIR/still.png" || true

echo "Done."
