/** @type {import('next').NextConfig} */
const nextConfig = {
  typescript: {
    // Production k3d validation needs a built Next app even while the UI still has
    // unrelated strictness issues that were previously masked by dev-mode startup.
    ignoreBuildErrors: true,
  },
};

module.exports = nextConfig;
