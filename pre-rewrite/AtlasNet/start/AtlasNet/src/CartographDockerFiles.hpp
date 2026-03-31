#include "Misc/String_utils.hpp"
#include "MiscDockerFiles.hpp"

DOCKER_FILE_DEF CartographDockerFile_old =
	MacroParse(R"(

# ---------- Base image with Node ----------
FROM node:22-alpine AS base

# All paths inside the container will be relative to /app
WORKDIR /app

# ---------- Install dependencies ----------
# deps stage
FROM base AS deps
ARG CartographPath

WORKDIR /app/web

# Copy main package.json
COPY --from=atlasnetsdk:latest runtime/cartograph/web/package*.json ./
# Copy native-server package.json correctly relative to /app/web
COPY --from=atlasnetsdk:latest runtime/cartograph/web/native-server/package*.json ./native-server/

# Install dependencies (postinstall will now find native-server correctly)
RUN npm install


# ---------- Build the Next.js app ----------
FROM base AS builder

WORKDIR /app/web
ARG CartographPath

# Copy the rest of the web project
COPY --from=atlasnetsdk:latest runtime/cartograph/web/ ./

# Remove local node_modules just in case
RUN rm -rf ./node_modules ./.next

# Copy node_modules from deps stage
COPY --from=deps /app/web/node_modules ./node_modules

# Build Next.js
RUN npm run build


# ---------- Runtime image ----------
FROM node:22-bookworm AS runner  
# <-- changed to Ubuntu-based
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        g++-12 \
        gcc-12 \
        libstdc++6 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100 && \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100 && \
    rm -rf /var/lib/apt/lists/*


${GET_RUN_PKGS}  # make sure these are apt packages if needed
WORKDIR /app/web
ENV NODE_ENV=development

# Copy built app + node_modules
COPY --from=builder /app/web ./

# Next.js default port
EXPOSE 3000
#EXPOSE 9229  # optional if you want debugging

# Start your Next.js app using npm run dev
CMD ["npm", "run", "dev:all"]



)",
			   {
				{"GET_RUN_PKGS", GET_REQUIRED_RUN_PKGS}});