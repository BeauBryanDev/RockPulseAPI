import React, { useState, useRef, useEffect } from 'react';
import { motion } from 'framer-motion';
import { ZoomIn, ZoomOut, Maximize2, Crosshair, Target } from 'lucide-react';
import CyberButton from './CyberButton';

interface Detection {
    id: number;
    bbox: { x: number; y: number; w: number; h: number };
    confidence: number;
    label?: string;
    [key: string]: any;
}

interface DetectionViewerProps {
    imageUrl: string;
    detections: Detection[];
    onRockSelect?: (rock: Detection) => void;
    isLoading?: boolean;
}

const DetectionViewer: React.FC<DetectionViewerProps> = ({
    imageUrl,
    detections,
    onRockSelect,
    isLoading = false
}) => {
    const [zoom, setZoom] = useState(1);
    const [selectedId, setSelectedId] = useState<number | null>(null);
    const containerRef = useRef<HTMLDivElement>(null);
    const imageRef = useRef<HTMLImageElement>(null);

    const handleRockClick = (rock: Detection) => {
        setSelectedId(rock.id);
        if (onRockSelect) onRockSelect(rock);
    };

    return (
        <div className="relative w-full aspect-video bg-black/40 border border-primary/20 overflow-hidden group rounded-sm shadow-neon-inner">
            {/* HUD Overlays */}
            <div className="absolute inset-0 pointer-events-none z-10">
                <div className="absolute top-4 left-4 flex gap-2">
                    <div className="px-2 py-1 bg-primary/20 border border-primary/40 rounded text-[8px] font-black text-primary tracking-widest uppercase italic animate-pulse">
                        LIVE_INFERENCE_DATA
                    </div>
                    <div className="px-2 py-1 bg-black/40 border border-white/10 rounded text-[8px] font-mono text-white/40 tracking-widest uppercase">
                        FPS: 12.4
                    </div>
                </div>

                {/* Corner Reticles */}
                <div className="absolute top-0 left-0 w-8 h-8 border-t border-l border-primary/40" />
                <div className="absolute top-0 right-0 w-8 h-8 border-t border-r border-primary/40" />
                <div className="absolute bottom-0 left-0 w-8 h-8 border-b border-l border-primary/40" />
                <div className="absolute bottom-0 right-0 w-8 h-8 border-b border-r border-primary/40" />

                {/* Center Crosshair */}
                <div className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 opacity-20">
                    <Crosshair className="text-primary" size={40} />
                </div>
            </div>

            {/* Main Image Viewport */}
            <div
                ref={containerRef}
                className="w-full h-full cursor-crosshair relative overflow-hidden"
            >
                {isLoading ? (
                    <div className="absolute inset-0 flex flex-col items-center justify-center bg-black/60 z-20">
                        <div className="w-12 h-12 border-2 border-primary/20 border-t-primary rounded-full animate-spin shadow-neon" />
                        <span className="mt-4 text-[10px] font-black text-primary tracking-[0.3em] uppercase">Processing Tensor...</span>
                    </div>
                ) : (
                    <motion.div
                        animate={{ scale: zoom }}
                        transition={{ type: "spring", stiffness: 300, damping: 30 }}
                        className="relative inline-block min-w-full"
                    >
                        <img
                            ref={imageRef}
                            src={imageUrl}
                            alt="Detection Source"
                            className="max-w-none w-full grayscale-[0.2] brightness-[0.8] contrast-[1.2]"
                        />

                        {/* SVG Overlay for Bounding Boxes */}
                        <svg
                            className="absolute inset-0 pointer-events-none w-full h-full"
                            viewBox={`0 0 640 640`} // Assuming 640x640 input resolution as per README
                            preserveAspectRatio="xMidYMid meet"
                        >
                            {detections.map((det) => (
                                <g
                                    key={det.id}
                                    className="pointer-events-auto cursor-pointer group/det"
                                    onClick={() => handleRockClick(det)}
                                >
                                    <rect
                                        x={det.bbox.x}
                                        y={det.bbox.y}
                                        width={det.bbox.w}
                                        height={det.bbox.h}
                                        className={`
                      fill-none stroke-[1.5] transition-all duration-300
                      ${selectedId === det.id
                                                ? 'stroke-primary drop-shadow-[0_0_10px_rgba(0,242,255,0.8)]'
                                                : 'stroke-primary/40 group-hover/det:stroke-primary group-hover/det:stroke-[2]'}
                    `}
                                    />
                                    {/* Confidence ID Tag */}
                                    {(selectedId === det.id || zoom > 1.5) && (
                                        <g transform={`translate(${det.bbox.x}, ${det.bbox.y - 12})`}>
                                            <rect width="60" height="12" fill="rgba(0, 242, 255, 0.9)" />
                                            <text
                                                x="4"
                                                y="9"
                                                className="fill-black font-black italic text-[9px]"
                                            >
                                                ID:{det.id} [{(det.confidence * 100).toFixed(0)}%]
                                            </text>
                                        </g>
                                    )}
                                </g>
                            ))}
                        </svg>
                    </motion.div>
                )}
            </div>

            {/* Control Bar */}
            <div className="absolute bottom-4 left-1/2 -translate-x-1/2 bg-secondary/80 backdrop-blur-md border border-primary/20 p-2 rounded-full flex gap-4 z-20 opacity-0 group-hover:opacity-100 transition-opacity">
                <button onClick={() => setZoom(prev => Math.min(prev + 0.5, 4))} className="text-primary/60 hover:text-primary transition-colors">
                    <ZoomIn size={18} />
                </button>
                <div className="w-[1px] h-4 bg-primary/10 self-center" />
                <button onClick={() => setZoom(1)} className="text-primary/60 hover:text-primary transition-colors">
                    <Target size={18} />
                </button>
                <div className="w-[1px] h-4 bg-primary/10 self-center" />
                <button onClick={() => setZoom(prev => Math.max(prev - 0.5, 1))} className="text-primary/60 hover:text-primary transition-colors">
                    <ZoomOut size={18} />
                </button>
            </div>

            {/* Metadata Panel Overlay (Left) */}
            <div className="absolute bottom-10 left-6 z-20 pointer-events-none opacity-0 group-hover:opacity-100 transition-opacity duration-500">
                <div className="flex flex-col gap-1">
                    <div className="flex items-center gap-2">
                        <div className="w-1 h-3 bg-primary" />
                        <span className="text-[9px] font-black text-primary/80 uppercase tracking-widest italic">RESOLVE: 640x640.TSR</span>
                    </div>
                    <div className="flex items-center gap-2">
                        <div className="w-1 h-3 bg-primary/40" />
                        <span className="text-[9px] font-black text-primary/40 uppercase tracking-widest italic">STREAM_ID: //NORTH_PIT_02</span>
                    </div>
                </div>
            </div>
        </div>
    );
};

export default DetectionViewer;
