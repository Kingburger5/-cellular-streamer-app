"use client";

import { useMemo } from 'react';
import type { FileContent, DataPoint } from '@/lib/types';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { ScrollArea } from '@/components/ui/scroll-area';
import { BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer, LineChart, Line } from 'recharts';
import { Satellite, Thermometer, Send, FileWarning } from 'lucide-react';

const parseData = (file: FileContent): DataPoint[] | null => {
  try {
    if (file.extractedData) {
        return file.extractedData;
    }
    if (file.extension === '.json') {
      const data = JSON.parse(file.content);
      return Array.isArray(data) ? data : [];
    }
    if (file.extension === '.csv') {
      const rows = file.content.split('\n').filter(Boolean);
       if (rows.length < 2) return [];
      const headers = rows[0].split(',').map(h => h.trim());
      const body = rows.slice(1);
      return body.map(row => {
        const values = row.split(',');
        const obj: any = {};
        headers.forEach((header, i) => {
          const value = values[i]?.trim();
          const numValue = parseFloat(value);
          obj[header] = isNaN(numValue) ? value : numValue;
        });
        return obj as DataPoint;
      });
    }
  } catch (e) {
    console.error("Failed to parse data:", e);
    return null;
  }
  return null;
};


export function DataVisualizer({ data: propData, rawFileContent }: { data: DataPoint[] | null, rawFileContent?: FileContent | null }) {
  const data = useMemo(() => {
    if (propData) return propData;
    if (rawFileContent) return parseData(rawFileContent);
    return null;
  }, [propData, rawFileContent]);

  if (data === null) {
    return (
      <div className="flex flex-col items-center justify-center h-full text-muted-foreground p-4 text-center">
        <FileWarning className="w-12 h-12 mb-4" />
        <h3 className="font-semibold">No Visualization Available</h3>
        <p className="text-sm">This file type cannot be visualized as data.</p>
        <p className="text-xs mt-2">Try the 'Pretty' or 'Raw' tabs for other views.</p>
      </div>
    );
  }
  
  if (data.length === 0) {
      return (
      <div className="flex flex-col items-center justify-center h-full text-muted-foreground p-4 text-center">
        <FileWarning className="w-12 h-12 mb-4" />
        <h3 className="font-semibold">Could not parse data</h3>
         <p className="text-sm">Ensure the file is valid JSON or CSV or a WAV with embedded metadata. Headers should include: timestamp, latitude, longitude, temperature, flybys.</p>
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
                {latestLocation ? (
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
                     <p className="text-3xl font-bold">{totalFlybys}</p>
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

      </div>
    </ScrollArea>
  );
}
