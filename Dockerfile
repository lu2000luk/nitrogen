FROM debian:stable-slim

RUN apt-get update && apt-get install -y --no-install-recommends ffmpeg ca-certificates curl libboost-all-dev wget \
    && rm -rf /var/lib/apt/lists/*

RUN wget -O /usr/local/bin/yt-dlp https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp \
    && chmod +x /usr/local/bin/yt-dlp

RUN wget -O /usr/local/bin/nitrogen-server https://cdn.lu2000luk.com/bin/nitrogen-server-3 \
    && chmod +x /usr/local/bin/nitrogen-server

EXPOSE 3070
ENTRYPOINT ["/usr/local/bin/nitrogen-server","--host","0.0.0.0","--use-local-yt-dlp","--no-download-yt-dlp", "--use-local-ffmpeg","--no-download-ffmpeg"]
