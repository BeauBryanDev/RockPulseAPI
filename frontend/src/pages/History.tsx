import React from 'react';
import { Calendar, Search, Filter, ArrowRight, Layers, FileText, Download, MoreVertical } from 'lucide-react';
import CyberCard from '../components/CyberCard';
import CyberButton from '../components/CyberButton';

const MOCK_HISTORY = [
    { id: '1', date: '2026-04-26 12:45', job_id: '52fb3f63', rock_count: 55, d50: 5.12, status: 'COMPLETE', thumbnail: 'https://images.unsplash.com/photo-1590483734731-39655f472898?q=80&w=200&auto=format&fit=crop' },
    { id: '2', date: '2026-04-26 11:20', job_id: '940be6c1', rock_count: 42, d50: 4.85, status: 'COMPLETE', thumbnail: 'https://images.unsplash.com/photo-1518709268805-4e9042af9f23?q=80&w=200&auto=format&fit=crop' },
    { id: '3', date: '2026-04-25 23:15', job_id: 'a4944b09', rock_count: 68, d50: 6.21, status: 'COMPLETE', thumbnail: 'https://images.unsplash.com/photo-1576086213369-97a306d36557?q=80&w=200&auto=format&fit=crop' },
    { id: '4', date: '2026-04-25 18:40', job_id: 'ebbf3037', rock_count: 31, d50: 3.92, status: 'COMPLETE', thumbnail: 'https://images.unsplash.com/photo-1445217143695-46712403d776?q=80&w=200&auto=format&fit=crop' },
    { id: '5', date: '2026-04-24 14:05', job_id: 'e91c742d', rock_count: 45, d50: 5.44, status: 'COMPLETE', thumbnail: 'https://images.unsplash.com/photo-1589993356061-0774780562e1?q=80&w=200&auto=format&fit=crop' },
    { id: '6', date: '2026-04-24 09:30', job_id: '94a37e0c', rock_count: 52, d50: 5.10, status: 'COMPLETE', thumbnail: 'https://images.unsplash.com/photo-1578328819058-b69f3a3b0f6b?q=80&w=200&auto=format&fit=crop' },
];

const History = () => {
    return (
        <div className="space-y-8 max-w-7xl mx-auto pb-20">
            {/* Header & Filters */}
            <div className="flex flex-col md:flex-row md:items-end justify-between gap-6">
                <div className="space-y-2">
                    <h2 className="text-3xl font-black italic text-glow tracking-tighter uppercase">Job_Database</h2>
                    <p className="text-[10px] text-primary/40 font-mono tracking-widest uppercase">Archive_Explorer // Total_Records: 1,482</p>
                </div>

                <div className="flex flex-wrap items-center gap-3">
                    <div className="relative group">
                        <Search className="absolute left-3 top-1/2 -translate-y-1/2 text-primary/40 group-hover:text-primary/60" size={16} />
                        <input
                            type="text"
                            placeholder="SEARCH_BY_JOB_ID_OR_DATE..."
                            className="bg-secondary/80 border border-primary/20 pl-10 pr-4 py-2 text-[10px] font-mono tracking-widest text-primary focus:outline-none focus:border-primary/50 w-64 uppercase"
                        />
                    </div>
                    <CyberButton variant="outline" size="sm" icon={<Filter size={14} />}>Filters</CyberButton>
                    <CyberButton variant="outline" size="sm" icon={<Calendar size={14} />}>Range</CyberButton>
                </div>
            </div>

            {/* Stats Breakdown */}
            <div className="grid grid-cols-1 lg:grid-cols-4 gap-4">
                <div className="p-4 bg-primary/5 border border-primary/10 flex items-center justify-between">
                    <div>
                        <p className="text-[8px] font-black text-primary/40 uppercase">Session_Storage</p>
                        <p className="text-xl font-black italic text-primary/80">14.2 GB</p>
                    </div>
                    <Database size={24} className="text-primary/20" />
                </div>
                <div className="p-4 bg-primary/5 border border-primary/10 flex items-center justify-between">
                    <div>
                        <p className="text-[8px] font-black text-primary/40 uppercase">Total_Sequences</p>
                        <p className="text-xl font-black italic text-primary/80">482</p>
                    </div>
                    <Layers size={24} className="text-primary/20" />
                </div>
                <div className="p-4 bg-primary/5 border border-primary/10 flex items-center justify-between">
                    <div>
                        <p className="text-[8px] font-black text-primary/40 uppercase">Reports_Generated</p>
                        <p className="text-xl font-black italic text-primary/80">1,240</p>
                    </div>
                    <FileText size={24} className="text-primary/20" />
                </div>
                <div className="p-4 bg-primary/5 border border-primary/10 flex items-center justify-between">
                    <div>
                        <p className="text-[8px] font-black text-primary/40 uppercase">Avg_Rocks_Session</p>
                        <p className="text-xl font-black italic text-primary/80">48.6</p>
                    </div>
                    <Activity size={24} className="text-primary/20" />
                </div>
            </div>

            {/* History Grid */}
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
                {MOCK_HISTORY.map((job) => (
                    <CyberCard
                        key={job.id}
                        title={`JOB_ID: //${job.job_id}`}
                        subtitle={job.date}
                        className="group/card"
                    >
                        <div className="space-y-4">
                            {/* Thumbnail Frame */}
                            <div className="relative aspect-video bg-black rounded-sm border border-primary/10 overflow-hidden group-hover/card:border-primary/40 transition-colors">
                                <img src={job.thumbnail} alt="Job Result" className="w-full h-full object-cover opacity-60 group-hover/card:scale-105 transition-transform" />
                                <div className="absolute inset-0 bg-gradient-to-t from-black/80 via-transparent to-transparent" />

                                {/* Status Overlay */}
                                <div className="absolute top-2 right-2 px-2 py-0.5 bg-primary/20 border border-primary/40 text-[8px] font-black text-primary italic">
                                    {job.status}
                                </div>
                            </div>

                            {/* Summary Stats */}
                            <div className="grid grid-cols-2 gap-4">
                                <div>
                                    <p className="text-[9px] text-primary/40 font-black uppercase">Rock_Count</p>
                                    <p className="text-lg font-black text-primary/80 italic">{job.rock_count}</p>
                                </div>
                                <div>
                                    <p className="text-[9px] text-primary/40 font-black uppercase">Median_D50</p>
                                    <p className="text-lg font-black text-primary/80 italic">{job.d50} cm</p>
                                </div>
                            </div>

                            {/* Actions */}
                            <div className="flex gap-2 pt-2">
                                <CyberButton size="sm" className="flex-1" icon={<ArrowRight size={14} />}>View_Details</CyberButton>
                                <button className="p-2 bg-secondary border border-primary/10 text-primary/40 hover:text-primary transition-colors hover:border-primary/30">
                                    <Download size={16} />
                                </button>
                                <button className="p-2 bg-secondary border border-primary/10 text-primary/40 hover:text-primary transition-colors hover:border-primary/30">
                                    <MoreVertical size={16} />
                                </button>
                            </div>
                        </div>
                    </CyberCard>
                ))}
            </div>

            {/* Pagination Placeholder */}
            <div className="flex justify-center pt-10">
                <div className="flex gap-2 p-1 bg-secondary border border-primary/10 rounded">
                    <button className="px-3 py-1 text-xs font-bold text-primary/40 hover:text-primary transition-colors">Prev</button>
                    <div className="px-3 py-1 bg-primary/10 text-primary text-xs font-bold border border-primary/30">01</div>
                    <button className="px-3 py-1 text-xs font-bold text-primary/40 hover:text-primary transition-colors">02</button>
                    <button className="px-3 py-1 text-xs font-bold text-primary/40 hover:text-primary transition-colors">03</button>
                    <span className="px-2 self-center opacity-30">...</span>
                    <button className="px-3 py-1 text-xs font-bold text-primary/40 hover:text-primary transition-colors">Next</button>
                </div>
            </div>
        </div>
    );
};

export default History;
