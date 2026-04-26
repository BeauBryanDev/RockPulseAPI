import React, { useState } from 'react';
import { Upload, Activity, Zap, BarChart3, Database, ChevronRight, Info, AlertTriangle } from 'lucide-react';
import CyberCard from '../components/CyberCard';
import CyberButton from '../components/CyberButton';
import DetectionViewer from '../components/DetectionViewer';
import GranulometryChart from '../components/GranulometryChart';

// Mock data for initial preview
const MOCK_GRAN_DATA = [
    { d: 1.2, cp: 0.05 }, { d: 2.4, cp: 0.12 }, { d: 3.8, cp: 0.30 },
    { d: 5.1, cp: 0.50 }, { d: 6.2, cp: 0.65 }, { d: 7.4, cp: 0.80 },
    { d: 8.5, cp: 0.92 }, { d: 9.8, cp: 1.00 }
];

const MOCK_DETECTIONS = [
    { id: 1, bbox: { x: 120, y: 80, w: 95, h: 72 }, confidence: 0.94, area_cm2: 18.4 },
    { id: 2, bbox: { x: 300, y: 150, w: 80, h: 60 }, confidence: 0.88, area_cm2: 12.1 },
    { id: 3, bbox: { x: 450, y: 220, w: 110, h: 90 }, confidence: 0.91, area_cm2: 25.6 }
];

const Dashboard = () => {
    const [isUploading, setIsUploading] = useState(false);
    const [hasResult, setHasResult] = useState(false);

    const handleUpload = () => {
        setIsUploading(true);
        // Simulate API delay
        setTimeout(() => {
            setIsUploading(false);
            setHasResult(true);
        }, 2000);
    };

    return (
        <div className="space-y-8 max-w-7xl mx-auto">
            {/* Top Banner / Stats */}
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
                <CyberCard title="Total_Detections" subtitle="Session_Delta: +12%" className="border-l-4 border-l-primary">
                    <div className="flex items-end justify-between">
                        <div>
                            <span className="text-4xl font-black italic text-glow">1,284</span>
                            <p className="text-[10px] text-primary/40 font-mono tracking-widest mt-1">TOTAL_ROCKS_IDENTIFIED</p>
                        </div>
                        <Activity className="text-primary/20" size={32} />
                    </div>
                </CyberCard>

                <CyberCard title="Avg_Equivalent_Diam" subtitle="Unit: CM" className="border-l-4 border-l-primary/40">
                    <div className="flex items-end justify-between">
                        <div>
                            <span className="text-4xl font-black italic text-glow">5.12</span>
                            <p className="text-[10px] text-primary/40 font-mono tracking-widest mt-1">D50_GRANULOMETRY_MEDIAN</p>
                        </div>
                        <BarChart3 className="text-primary/20" size={32} />
                    </div>
                </CyberCard>

                <CyberCard title="Conveyor_Density" subtitle="Material: Coal_Bitum" className="border-l-4 border-l-primary/40">
                    <div className="flex items-end justify-between">
                        <div>
                            <span className="text-4xl font-black italic text-glow">1,500</span>
                            <p className="text-[10px] text-primary/40 font-mono tracking-widest mt-1">KG/M³_CALIBRATED</p>
                        </div>
                        <Zap className="text-primary/20" size={32} />
                    </div>
                </CyberCard>

                <CyberCard title="System_Load" subtitle="ORT_Execution_Time" className="border-l-4 border-l-primary/40">
                    <div className="flex items-end justify-between">
                        <div>
                            <span className="text-4xl font-black italic text-glow">142</span>
                            <p className="text-[10px] text-primary/40 font-mono tracking-widest mt-1">MS_PER_INFERENCE</p>
                        </div>
                        <Database className="text-primary/20" size={32} />
                    </div>
                </CyberCard>
            </div>

            <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
                {/* Main Detection Area */}
                <div className="lg:col-span-2 space-y-6">
                    <CyberCard title="Inference_Engine_Output" subtitle="Real-time_Metric_Validation">
                        {!hasResult ? (
                            <div className="flex flex-col items-center justify-center py-20 border-2 border-dashed border-primary/20 bg-primary/5 rounded-sm group hover:border-primary/40 transition-colors cursor-pointer" onClick={handleUpload}>
                                <div className="w-16 h-16 bg-primary/10 rounded-full flex items-center justify-center mb-6 group-hover:scale-110 transition-transform shadow-neon">
                                    <Upload className="text-primary" size={32} />
                                </div>
                                <h4 className="text-sm font-black text-primary tracking-widest italic uppercase">Upload Tactical Image</h4>
                                <p className="text-[10px] text-primary/40 mt-2 font-mono">SUPPORTED: .JPG, .PNG, .BMP (MAX 20MB)</p>

                                <div className="mt-10 flex gap-4">
                                    <CyberButton size="sm" onClick={handleUpload} disabled={isUploading}>
                                        {isUploading ? 'Initializing...' : 'Select Source'}
                                    </CyberButton>
                                </div>
                            </div>
                        ) : (
                            <div className="space-y-6">
                                <DetectionViewer
                                    imageUrl="https://images.unsplash.com/photo-1590483734731-39655f472898?q=80&w=2670&auto=format&fit=crop" // Sample high-res rock texture
                                    detections={MOCK_DETECTIONS}
                                    isLoading={isUploading}
                                />

                                <div className="flex justify-between items-center">
                                    <div className="flex gap-4">
                                        <CyberButton variant="outline" size="sm" onClick={() => setHasResult(false)}>Clear_Session</CyberButton>
                                        <CyberButton size="sm">Export_JSON</CyberButton>
                                    </div>
                                    <div className="flex items-center gap-2 text-primary/60">
                                        <AlertTriangle size={14} className="animate-pulse" />
                                        <span className="text-[9px] font-black tracking-widest">CALIB_K: 0.045 CM/PX</span>
                                    </div>
                                </div>
                            </div>
                        )}
                    </CyberCard>
                </div>

                {/* Side Analytics */}
                <div className="space-y-6">
                    <CyberCard title="Granulometry_Curve" subtitle="Cumulative_Pass_Volume">
                        <GranulometryChart
                            data={MOCK_GRAN_DATA}
                            d50={5.12}
                        />
                        <div className="grid grid-cols-3 mt-6 gap-2">
                            <div className="p-3 bg-secondary/60 border border-primary/10 text-center">
                                <p className="text-[8px] text-primary/40 font-black uppercase">D30</p>
                                <p className="text-lg font-black italic text-primary">3.8</p>
                            </div>
                            <div className="p-3 bg-primary/10 border border-primary/40 text-center shadow-neon">
                                <p className="text-[8px] text-primary/60 font-black uppercase">D50</p>
                                <p className="text-lg font-black italic text-primary">5.1</p>
                            </div>
                            <div className="p-3 bg-secondary/60 border border-primary/10 text-center">
                                <p className="text-[8px] text-primary/40 font-black uppercase">D80</p>
                                <p className="text-lg font-black italic text-primary">7.4</p>
                            </div>
                        </div>
                    </CyberCard>

                    <CyberCard title="Session_Log" subtitle="Event_Sequence_04-26" className="max-h-64 overflow-hidden flex flex-col">
                        <div className="space-y-3 font-mono text-[9px] flex-1 overflow-y-auto custom-scrollbar pr-2">
                            <div className="flex gap-2">
                                <span className="text-primary/40">12:43:01</span>
                                <span className="text-primary/80">AUTH_SUCCESS // TOKEN_HASH_VERIFIED</span>
                            </div>
                            <div className="flex gap-2">
                                <span className="text-primary/40">12:44:12</span>
                                <span className="text-primary/80">CONVEYOR_LINKED // BELT_01_SOUTH</span>
                            </div>
                            <div className="flex gap-2">
                                <span className="text-primary/40">12:45:55</span>
                                <span className="text-primary/80 italic text-primary underline">INFERENCE_START // JOB_52fb3f</span>
                            </div>
                            <div className="flex gap-2">
                                <span className="text-primary/40">12:46:02</span>
                                <span className="text-primary/80">INFERENCE_COMPLETE // 55_ROCKS_FOUND</span>
                            </div>
                            <div className="flex gap-2">
                                <span className="text-primary/40">12:46:10</span>
                                <span className="text-primary/80">METROLOGY_CALC // k_045_cm_px</span>
                            </div>
                            <div className="flex gap-2 pt-2 items-center justify-center opacity-30">
                                <ChevronRight size={12} className="rotate-90" />
                            </div>
                        </div>
                    </CyberCard>
                </div>
            </div>
        </div>
    );
};

export default Dashboard;
