/** @type {import('tailwindcss').Config} */
export default {
    content: [
        "./index.html",
        "./src/**/*.{js,ts,jsx,tsx}",
    ],
    theme: {
        extend: {
            colors: {
                background: "#050b0d", // Ultra deep dark blue-black
                foreground: "#e0f7fa",
                primary: {
                    DEFAULT: "#00f2ff", // Cyan Neon
                    dark: "#008899",
                    glow: "rgba(0, 242, 255, 0.5)",
                },
                secondary: {
                    DEFAULT: "#0a1a1f", // Deep tactical cyan
                    foreground: "#00f2ff",
                },
                accent: {
                    DEFAULT: "#00f2ff",
                    glow: "rgba(0, 242, 255, 0.3)",
                },
                card: {
                    DEFAULT: "rgba(6, 18, 21, 0.8)",
                    border: "rgba(0, 242, 255, 0.2)",
                },
                panel: "#0d2127",
                border: "rgba(0, 242, 255, 0.1)",
                ring: "#00f2ff",
            },
            boxShadow: {
                'neon': '0 0 5px rgba(0, 242, 255, 0.5), 0 0 20px rgba(0, 242, 255, 0.2)',
                'neon-strong': '0 0 10px rgba(0, 242, 255, 0.8), 0 0 30px rgba(0, 242, 255, 0.4)',
                'neon-inner': 'inset 0 0 15px rgba(0, 242, 255, 0.1)',
            },
            backgroundImage: {
                'cyber-grid': 'radial-gradient(circle, rgba(0, 242, 255, 0.05) 1px, transparent 1px)',
                'scanlines': 'linear-gradient(to bottom, transparent 50%, rgba(0, 242, 255, 0.02) 50.1%)',
            },
            backgroundSize: {
                'grid-20': '20px 20px',
                'scanline-4': '100% 4px',
            },
            animation: {
                'scan': 'scan 8s linear infinite',
                'flicker': 'flicker 2s infinite',
                'glitch': 'glitch 1s infinite',
            },
            keyframes: {
                scan: {
                    '0%': { backgroundPosition: '0 0' },
                    '100%': { backgroundPosition: '0 100%' },
                },
                flicker: {
                    '0%, 19%, 21%, 23%, 25%, 54%, 56%, 100%': { opacity: 1 },
                    '20%, 24%, 55%': { opacity: 0.8 },
                },
                glitch: {
                    '0%': { transform: 'translate(0)' },
                    '20%': { transform: 'translate(-2px, 2px)' },
                    '40%': { transform: 'translate(-2px, -2px)' },
                    '60%': { transform: 'translate(2px, 2px)' },
                    '80%': { transform: 'translate(2px, -2px)' },
                    '100%': { transform: 'translate(0)' },
                },
            }
        },
    },
    plugins: [],
}
