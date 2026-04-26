import React from 'react';
import { motion } from 'framer-motion';

interface CyberCardProps {
    children: React.ReactNode;
    title?: string;
    subtitle?: string;
    className?: string;
    hoverGlow?: boolean;
}

const CyberCard: React.FC<CyberCardProps> = ({
    children,
    title,
    subtitle,
    className = "",
    hoverGlow = true
}) => {
    return (
        <motion.div
            initial={{ opacity: 0, scale: 0.98 }}
            animate={{ opacity: 1, scale: 1 }}
            whileHover={hoverGlow ? { scale: 1.01, boxShadow: '0 0 25px rgba(0, 242, 255, 0.15)' } : {}}
            className={`cyber-border overflow-hidden relative group ${className}`}
        >
            {/* Decorative corner accents */}
            <div className="absolute top-0 left-0 w-4 h-[2px] bg-primary/40" />
            <div className="absolute top-0 left-0 w-[2px] h-4 bg-primary/40" />
            <div className="absolute bottom-0 right-0 w-4 h-[2px] bg-primary/40" />
            <div className="absolute bottom-0 right-0 w-[2px] h-4 bg-primary/40" />

            {/* Title area if provided */}
            {(title || subtitle) && (
                <div className="px-6 py-4 border-b border-primary/10 bg-primary/5 flex justify-between items-center">
                    <div>
                        {title && <h3 className="text-[10px] font-black uppercase tracking-[0.3em] text-primary/80 text-glow italic">{title}</h3>}
                        {subtitle && <p className="text-[8px] text-primary/40 font-mono tracking-widest mt-0.5">{subtitle}</p>}
                    </div>
                    <div className="w-1.5 h-1.5 rounded-full bg-primary/20 group-hover:bg-primary shadow-neon transition-colors" />
                </div>
            )}

            {/* Content */}
            <div className="p-6 relative">
                {children}

                {/* Subtle decorative grid in corners */}
                <div className="absolute top-2 right-2 w-8 h-8 opacity-5 pointer-events-none">
                    <div className="grid grid-cols-2 gap-1 h-full w-full">
                        {[...Array(4)].map((_, i) => <div key={i} className="bg-primary rounded-full" />)}
                    </div>
                </div>
            </div>

            {/* Glitch overlay on hover */}
            <div className="absolute inset-0 bg-primary/2 opacity-0 group-hover:opacity-100 transition-opacity pointer-events-none" />
        </motion.div>
    );
};

export default CyberCard;
