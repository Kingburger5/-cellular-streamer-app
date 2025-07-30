import type {NextConfig} from 'next';

const nextConfig: NextConfig = {
  /* config options here */
  typescript: {
    ignoreBuildErrors: true,
  },
  eslint: {
    ignoreDuringBuilds: true,
  },
  images: {
    remotePatterns: [
      {
        protocol: 'https',
        hostname: 'placehold.co',
        port: '',
        pathname: '/**',
      },
    ],
  },
  // Required for handling secure headers in a proxied environment
  async headers() {
    return [
      {
        source: '/:path*',
        headers: [
          {
            key: 'x-forwarded-proto',
            value: 'https',
          },
        ],
      },
    ]
  },
};

export default nextConfig;
