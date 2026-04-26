import React from 'react';
import { motion } from 'framer-motion';

interface CyberButtonProps {
    children: React.ReactNode;
    onClick?: () => void;
    className?: string;
    variant?: 'primary' | 'outline' | 'ghost' | 'danger';
    size?: 'sm' | 'md' | 'lg';
    disabled?: boolean;
    type?: 'button' | 'submit' | 'reset';
    icon?: React.ReactNode;
}

const CyberButton: React.FC<CyberButtonProps> = ({
    children,
    onClick,
    className = "",
    variant = 'primary',
    size = 'md',
    disabled = false,
    type = 'button',
    icon
}) => {
    const getVariantStyles = () => {
        switch (variant) {
            case 'primary':
                return 'bg-primary/20 border-primary/50 text-primary shadow-neon hover:bg-primary/30 hover:shadow-neon-strong active:scale-95';
            case 'outline':
                return 'bg-transparent border-primary/30 text-primary/80 hover:bg-primary/10 hover:border-primary/50';
            case 'ghost':
                return 'bg-transparent border-transparent text-primary/60 hover:text-primary hover:bg-primary/5';
            case 'danger':
                return 'bg-red-500/10 border-red-500/30 text-red-500 hover:bg-red-500/20 shadow-[0_0_10px_rgba(239,68,68,0.2)]';
            default:
                return '';
        }
    };

    const getSizeStyles = () => {
        switch (size) {
            case 'sm': return 'px-4 py-1.5 text-[9px] h-8';
            case 'md': return 'px-6 py-2.5 text-[10px] h-10';
            case 'lg': return 'px-10 py-3.5 text-xs h-12';
            default: return '';
        }
    };

    return (
        <motion.button
            whileHover={!disabled ? { skewX: -5 } : {}}
            whileTap={!disabled ? { skewX: 0, scale: 0.98 } : {}}
            type={type}
            disabled={disabled}
            onClick={onClick}
            className={`
        relative inline-flex items-center justify-center gap-2 border font-black uppercase tracking-[0.2em] transition-all duration-200 overflow-hidden
        ${getVariantStyles()}
        ${getSizeStyles()}
        ${disabled ? 'opacity-40 cursor-not-allowed filter grayscale' : 'cursor-pointer'}
        ${className}
      `}
            style={{
                clipPath: 'polygon(10px 0, 100% 0, 100% calc(100% - 10px), calc(100% - 10px) 100%, 0 100%, 0 10px)'
            }}
        >
            {/* Scanning light gradient effect */}
            <div className="absolute inset-0 bg-gradient-to-r from-transparent via-white/5 to-transparent -translate-x-full group-hover:translate-x-full transition-transform duration-1000" />

            {icon && <span className="opacity-80 group-hover:opacity-100">{icon}</span>}
            <span className="relative z-10 italic">{children}</span>

            {/* Decorative dot */}
            <div className="absolute bottom-0 right-[15px] w-1 h-1 bg-primary/40 group-hover:bg-primary transition-colors" />
        </motion.button>
    );
};

export default CyberButton;
