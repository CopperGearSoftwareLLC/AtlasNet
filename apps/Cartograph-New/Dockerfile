# ---------- Base image with Node ----------
FROM node:22-alpine AS base

# All paths inside the container will be relative to /app
WORKDIR /app

# ---------- Install dependencies ----------
FROM base AS deps

# Copy only package files first (better Docker cache)
COPY web/package*.json ./web/

WORKDIR /app/web
RUN npm install

# ---------- Build the Next.js app ----------
FROM base AS builder

WORKDIR /app/web

# Bring in node_modules from deps stage
COPY --from=deps /app/web/node_modules ./node_modules

# Copy the rest of your Next.js project from /web
COPY web/ ./

# Build for production
RUN npm run build

# ---------- Runtime image ----------
FROM node:22-alpine AS runner

WORKDIR /app/web
ENV NODE_ENV=production

# Copy built app + node_modules
COPY --from=builder /app/web ./

# Next.js default port
EXPOSE 3000

# Start your Next.js app
# (Assumes "start" script is defined in /web/package.json)
CMD ["npm", "start"]
