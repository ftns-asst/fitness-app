import react from '@vitejs/plugin-react'
import { defineConfig } from 'vite'

// https://vite.dev/config/
export default defineConfig({
  server: {
    proxy: {
      '/api': {target: 'http://localhost:8181', changeOrigin: true, ws: true},
    },
  },
  plugins: [react()],
  build: {
    outDir: '../backend/static/files',
  },
  base: '/admin/'
})
