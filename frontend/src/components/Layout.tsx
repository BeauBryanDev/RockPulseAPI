import { Outlet, Link, useLocation } from 'react-router-dom';
import { LayoutDashboard, History as HistoryIcon, Pickaxe, Settings, Shield, Activity, Cpu, Database } from 'lucide-react';
import { motion, AnimatePresence } from 'framer-motion';

const Layout = () => {
    const location = useLocation();

    const navItems = [
        { name: 'Dashboard', path: '/dashboard', icon: LayoutDashboard },
        { name: 'History', path: '/history', icon: HistoryIcon },
    ];

    return (
        <div className="flex h-screen bg-background text-foreground cyber-grid-bg relative custom-scrollbar">
            {/* Scanline effect layer */}
            <div className="scanline" />

            {/* Sidebar */}
            <aside className="w-64 border-r border-primary/20 bg-secondary/80 backdrop-blur-xl flex flex-col z-30 shadow-[5px_0_15px_rgba(0,0,0,0.5)]">
                <div className="p-8 flex flex-col items-center gap-4">
                    <motion.div
                        whileHover={{ scale: 1.05 }}
                        className="w-16 h-16 bg-secondary border-2 border-primary rounded-lg flex items-center justify-center shadow-neon relative group overflow-hidden"
                    >
                        <Pickaxe className="text-primary stroke-[2px] z-10" size={32} />
                        <div className="absolute inset-0 bg-primary/10 group-hover:bg-primary/20 transition-colors" />
                        <div className="absolute -bottom-1 -right-1 w-4 h-4 bg-primary rotate-45" />
                    </motion.div>
                    <div className="text-center">
                        <h1 className="font-black text-xl tracking-tighter uppercase text-glow italic">RockPulse<span className="text-primary/70">.api</span></h1>
                        <div className="h-[1px] w-full bg-gradient-to-r from-transparent via-primary/50 to-transparent my-1" />
                        <p className="text-[10px] text-primary/60 font-bold tracking-[0.3em] uppercase opacity-80">Tactical Granulometry</p>
                    </div>
                </div>

                <nav className="flex-1 px-4 py-8 space-y-4">
                    {navItems.map((item) => {
                        const Icon = item.icon;
                        const isActive = location.pathname === item.path;
                        return (
                            <Link
                                key={item.path}
                                to={item.path}
                                className={`flex items-center gap-4 px-4 py-4 rounded-md transition-all duration-300 group relative overflow-hidden ${isActive
                                        ? 'text-primary bg-primary/5'
                                        : 'text-primary/40 hover:text-primary hover:bg-primary/5'
                                    }`}
                            >
                                {/* Active Indicator Bar */}
                                {isActive && (
                                    <motion.div
                                        layoutId="activeBar"
                                        className="absolute left-0 top-0 bottom-0 w-[2px] bg-primary shadow-neon"
                                    />
                                )}

                                <Icon size={20} className={`transition-all duration-300 ${isActive ? 'drop-shadow-[0_0_5px_#00f2ff]' : 'opacity-60'}`} />
                                <span className="font-bold text-xs uppercase tracking-widest">{item.name}</span>

                                {/* Visual glitches on hover */}
                                <div className="absolute right-0 top-0 bottom-0 w-8 bg-gradient-to-l from-primary/5 to-transparent opacity-0 group-hover:opacity-100 transition-opacity" />
                            </Link>
                        );
                    })}
                </nav>

                <div className="mt-auto flex flex-col p-4 border-t border-primary/10 bg-black/40">
                    <div className="grid grid-cols-2 gap-2 mb-4">
                        <div className="flex flex-col p-2 bg-secondary border border-primary/10 rounded">
                            <span className="text-[8px] text-primary/40 uppercase font-black uppercase">CORE_TEMP</span>
                            <span className="text-xs font-mono text-primary/80">34.2 °C</span>
                        </div>
                        <div className="flex flex-col p-2 bg-secondary border border-primary/10 rounded">
                            <span className="text-[8px] text-primary/40 uppercase font-black uppercase">NET_LATENCY</span>
                            <span className="text-xs font-mono text-primary/80">12 ms</span>
                        </div>
                    </div>

                    <div className="flex items-center gap-3 px-2 py-2 text-primary/40 text-[10px] uppercase font-black cursor-pointer hover:text-primary transition-colors">
                        <Activity size={14} />
                        <span className="tracking-widest">Sys Diagnostics</span>
                    </div>

                    <div className="mt-4 p-3 bg-primary/5 border border-primary/20 rounded-sm">
                        <div className="flex items-center gap-2 mb-1">
                            <div className="w-1.5 h-1.5 rounded-full bg-primary animate-flicker shadow-neon" />
                            <span className="text-[9px] font-black uppercase tracking-tighter text-primary">Neural Engine Linked</span>
                        </div>
                        <div className="w-full bg-white/5 h-[3px] rounded-full overflow-hidden">
                            <motion.div
                                initial={{ width: "0%" }}
                                animate={{ width: "88%" }}
                                className="h-full bg-primary shadow-neon"
                            />
                        </div>
                    </div>
                </div>
            </aside>

            {/* Main Content */}
            <main className="flex-1 overflow-y-auto relative custom-scrollbar flex flex-col z-20">
                <header className="h-20 border-b border-primary/20 bg-background/80 backdrop-blur-md sticky top-0 z-40 px-10 flex items-center justify-between">
                    <div className="flex items-center gap-4">
                        <Cpu className="text-primary/40" size={18} />
                        <h2 className="text-[11px] font-black text-primary/60 uppercase tracking-[0.4em] italic drop-shadow-sm">
                            Terminal // {navItems.find(item => item.path === location.pathname)?.name || 'Root'}
                        </h2>
                    </div>

                    <div className="flex items-center gap-8">
                        <div className="hidden lg:flex items-center gap-4 border-x border-primary/10 px-8">
                            <div className="flex flex-col items-end">
                                <span className="text-[9px] text-primary/40 font-black uppercase">CURRENT_LOCATION</span>
                                <span className="text-xs font-bold text-primary tracking-widest uppercase">NORTH_PIT_A2</span>
                            </div>
                            <div className="w-[1px] h-8 bg-primary/20" />
                            <div className="flex flex-col items-end">
                                <span className="text-[9px] text-primary/40 font-black uppercase">AUTH_KEY</span>
                                <span className="text-xs font-mono text-primary/80">X-API-KEY: REDACTED</span>
                            </div>
                        </div>

                        <button className="flex items-center gap-3 group">
                            <div className="flex flex-col items-end mr-1">
                                <span className="text-[10px] font-bold text-foreground group-hover:text-primary transition-colors uppercase italic">B. CASTANO</span>
                                <span className="text-[8px] text-primary/50 font-mono tracking-tighter">ADMINISTRATOR</span>
                            </div>
                            <div className="w-10 h-10 rounded-sm border border-primary/30 relative overflow-hidden group-hover:border-primary transition-colors">
                                <div className="absolute inset-0 bg-primary/20 animate-pulse" />
                                <div className="absolute inset-[1px] bg-zinc-900 flex items-center justify-center">
                                    <Shield className="text-primary/40" size={18} />
                                </div>
                            </div>
                        </button>
                    </div>
                </header>

                <div className="p-10 flex-1 relative">
                    <AnimatePresence mode="wait">
                        <motion.div
                            key={location.pathname}
                            initial={{ opacity: 0, y: 10, filter: 'blur(4px)' }}
                            animate={{ opacity: 1, y: 0, filter: 'blur(0px)' }}
                            exit={{ opacity: 0, y: -10, filter: 'blur(4px)' }}
                            transition={{ duration: 0.3 }}
                        >
                            <Outlet />
                        </motion.div>
                    </AnimatePresence>
                </div>

                {/* Decorative footer corner */}
                <div className="absolute bottom-4 right-4 w-12 h-12 border-b-2 border-r-2 border-primary/20 pointer-events-none" />
            </main>
        </div>
    );
};

export default Layout;
