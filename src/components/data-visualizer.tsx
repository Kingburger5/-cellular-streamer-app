"use client";

import type { DataPoint } from '@/lib/types';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { ScrollArea } from '@/components/ui/scroll-area';
import { BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer, LineChart, Line } from 'recharts';
import { Satellite, Thermometer, Send, FileWarning } from 'lucide-react';

export function DataVisualizer({ data }: { data: DataPoint[] | null }) {

  if (!data || data.length === 0) {
    return (
      <div className="flex flex-col items-center justify-center h-full text-muted-foreground p-4 text-center">
        <FileWarning className="w-12 h-12 mb-4" />
        <h3 className="font-semibold">No Visualization Available</h3>
        <p className="text-sm">Could not extract any parsable data points from this file.</p>
        <p className="text-xs mt-2">Ensure the file contains valid metadata (e.g., GUANO for WAV, or standard CSV/JSON).</p>
      </div>
    );
  }

  const chartData = data.map(d => ({
    ...d,
    time: new Date(d.timestamp).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }),
  }));

  const avgTemperature = (data.reduce((acc, d) => acc + (d.temperature || 0), 0) / data.length).toFixed(1);
  const totalFlybys = data.reduce((acc, d) => acc + (d.flybys || 0), 0);
  const latestLocation = data.length > 0 ? data[data.length - 1] : null;

  return (
    <ScrollArea className="h-full">
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-4 p-1">
        
        <Card className="lg:col-span-2">
          <CardHeader>
            <CardTitle className="flex items-center gap-2"><Satellite /> Location Data</CardTitle>
          </CardHeader>
          <CardContent>
             <div className="h-64 w-full bg-muted rounded-lg flex items-center justify-center relative overflow-hidden">
                <div className="w-full h-px bg-border absolute top-1/2 left-0"></div>
                <div className="h-full w-px bg-border absolute left-1/2 top-0"></div>
                {latestLocation && latestLocation.latitude && latestLocation.longitude ? (
                    <>
                        <div className="text-center">
                            <p className="font-bold text-lg">{latestLocation.latitude.toFixed(4)}, {latestLocation.longitude.toFixed(4)}</p>
                            <p className="text-sm text-muted-foreground">Latest Reported Position</p>
                        </div>
                        <Send className="text-primary w-8 h-8 absolute" style={{ 
                            top: `calc(50% - ${latestLocation.latitude % 1 * 50}px - 16px)`, 
                            left: `calc(50% + ${latestLocation.longitude % 1 * 50}px - 16px)`
                        }}/>
                    </>
                ) : (
                    <p>No location data available.</p>
                )}
            </div>
          </CardContent>
        </Card>
        
        <div className="space-y-4">
            <Card>
                <CardHeader className="pb-2">
                    <CardTitle className="text-base font-normal flex items-center gap-2"><Thermometer /> Avg. Temperature</CardTitle>
                </CardHeader>
                <CardContent>
                    <p className="text-3xl font-bold">{avgTemperature}°C</p>
                </CardContent>
            </Card>
            <Card>
                 <CardHeader className="pb-2">
                    <CardTitle className="text-base font-normal flex items-center gap-2"><Send /> Total Fly-bys</CardTitle>
                </CardHeader>
                <CardContent>
                     <p className="text-3xl font-bold">{totalFlybys || 0}</p>
                </CardContent>
            </Card>
        </div>

        <Card className="lg:col-span-3">
          <CardHeader>
            <CardTitle>Temperature Over Time</CardTitle>
          </CardHeader>
          <CardContent className="h-72">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={chartData}>
                <CartesianGrid strokeDasharray="3 3" />
                <XAxis dataKey="time" />
                <YAxis unit="°C" />
                <Tooltip />
                <Legend />
                <Line type="monotone" dataKey="temperature" stroke="var(--color-chart-1)" strokeWidth={2} dot={false} />
              </LineChart>
            </ResponsiveContainer>
          </CardContent>
        </Card>

        {totalFlybys > 0 && (
          <Card className="lg:col-span-3">
            <CardHeader>
              <CardTitle>Fly-bys Over Time</CardTitle>
            </CardHeader>
            <CardContent className="h-72">
              <ResponsiveContainer width="100%" height="100%">
                <BarChart data={chartData}>
                  <CartesianGrid strokeDasharray="3 3" />
                  <XAxis dataKey="time" />
                  <YAxis />
                  <Tooltip />
                  <Legend />
                  <Bar dataKey="flybys" fill="var(--color-chart-2)" />
                </BarChart>
              </ResponsiveContainer>
            </CardContent>
          </Card>
        )}

      </div>
    </ScrollArea>
  );
}
