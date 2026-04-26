import React from 'react';
import { AreaChart, Area, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, ReferenceLine } from 'recharts';

interface DataPoint {
    d: number;
    cp: number;
}

interface GranulometryChartProps {
    data: DataPoint[];
    d30?: number;
    d50?: number;
    d80?: number;
    isLoading?: boolean;
}

const GranulometryChart: React.FC<GranulometryChartProps> = ({
    data,
    d30,
    d50,
    d80,
    isLoading = false
}) => {
    if (isLoading) {
        return (
            <div className="w-full h-64 flex items-center justify-center bg-primary/2 rounded border border-primary/10">
                <div className="w-8 h-8 border-2 border-primary/20 border-t-primary rounded-full animate-spin" />
            </div>
        );
    }

    return (
        <div className="w-full h-72 relative">
            <ResponsiveContainer width="100%" height="100%">
                <AreaChart
                    data={data}
                    margin={{ top: 10, right: 30, left: 0, bottom: 0 }}
                >
                    <defs>
                        <linearGradient id="cyanGradient" x1="0" y1="0" x2="0" y2="1">
                            <stop offset="5%" stopColor="#00f2ff" stopOpacity={0.3} />
                            <stop offset="95%" stopColor="#00f2ff" stopOpacity={0} />
                        </linearGradient>
                    </defs>
                    <CartesianGrid
                        strokeDasharray="3 3"
                        stroke="rgba(0, 242, 255, 0.05)"
                        vertical={false}
                    />
                    <XAxis
                        dataKey="d"
                        label={{ value: 'Size (cm)', position: 'insideBottomRight', offset: -5, fill: 'rgba(0, 242, 255, 0.4)', fontSize: 10 }}
                        stroke="rgba(0, 242, 255, 0.2)"
                        tick={{ fill: 'rgba(0, 242, 255, 0.6)', fontSize: 10, fontWeight: 'bold' }}
                    />
                    <YAxis
                        label={{ value: 'Passing (%)', angle: -90, position: 'insideLeft', fill: 'rgba(0, 242, 255, 0.4)', fontSize: 10 }}
                        stroke="rgba(0, 242, 255, 0.2)"
                        tick={{ fill: 'rgba(0, 242, 255, 0.6)', fontSize: 10, fontWeight: 'bold' }}
                        tickFormatter={(val) => `${(val * 100).toFixed(0)}%`}
                    />
                    <Tooltip
                        contentStyle={{
                            backgroundColor: '#0a1a1f',
                            border: '1px solid rgba(0, 242, 255, 0.3)',
                            borderRadius: '4px',
                            fontSize: '10px',
                            color: '#00f2ff'
                        }}
                        itemStyle={{ color: '#00f2ff' }}
                        formatter={(value: number) => [`${(value * 100).toFixed(1)}%`, 'Passing']}
                        labelFormatter={(label) => `Size: ${label} cm`}
                    />

                    {/* Reference Lines for D30, D50, D80 */}
                    {d50 && (
                        <ReferenceLine
                            x={d50}
                            stroke="rgba(0, 242, 255, 0.8)"
                            strokeDasharray="3 3"
                            label={{ value: `D50: ${d50}`, position: 'top', fill: '#00f2ff', fontSize: 9, fontWeight: 'bold' }}
                        />
                    )}

                    <Area
                        type="monotone"
                        dataKey="cp"
                        stroke="#00f2ff"
                        strokeWidth={3}
                        fillOpacity={1}
                        fill="url(#cyanGradient)"
                        animationDuration={1500}
                    />
                </AreaChart>
            </ResponsiveContainer>

            {/* Decorative chart overlay */}
            <div className="absolute top-0 right-0 p-2 opacity-20 pointer-events-none">
                <div className="text-[10px] font-mono text-primary italic">CURVE_ANALYSIS_v1.0</div>
            </div>
        </div>
    );
};

export default GranulometryChart;
