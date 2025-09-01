
"use client";

import type { DataPoint } from '@/lib/types';
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from '@/components/ui/card';
import { ScrollArea } from '@/components/ui/scroll-area';
import { BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer, LineChart, Line } from 'recharts';
import { Thermometer, Send, FileWarning, AlertTriangle, RadioTower, Zap, HardDrive, Loader, MapPin, Calendar, Clock, Gauge, Settings, SlidersHorizontal, GitCommitHorizontal } from 'lucide-react';
import { Button } from './ui/button';
import { useToast } from '@/hooks/use-toast';
import { useState, useTransition } from 'react';
import { Separator } from './ui/separator';

export function DataVisualizer({ data, fileName, isLoading }: { data: DataPoint[] | null, fileName: string, isLoading: boolean }) {
  const apiKey = process.env.NEXT_PUBLIC_GOOGLE_MAPS_API_KEY;
  
  if (isLoading) {
    return (
        <div className="flex flex-col items-center justify-center h-full text-muted-foreground p-4 text-center">
            <Loader className="w-12 h-12 mb-4 animate-spin" />
            <h3 className="font-semibold">Analyzing Data...</h3>
            <p className="text-sm">The AI is extracting structured data from the file's metadata.</p>
        </div>
    );
  }

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

  const formatHz = (hz: number) => {
    if (!hz && hz !== 0) return 'N/A';
    if (hz >= 1000) {
      return `${(hz / 1000).toFixed(1)} kHz`;
    }
    return `${hz} Hz`;
  }
  
  const latestData = data[0]; // Assuming the most relevant data is the first item

  const avgTemperature = (data.reduce((acc, d) => acc + (d.temperature || 0), 0) / data.length).toFixed(1);
  const totalFlybys = data.reduce((acc, d) => acc + (d.flybys || 0), 0);

  const StatCard = ({ icon, label, value, unit }: { icon: React.ReactNode, label: string, value: string | number, unit?: string}) => (
      <div className="flex items-center gap-4">
          <div className="bg-muted p-2 rounded-lg">
              {icon}
          </div>
          <div>
              <p className="text-sm text-muted-foreground">{label}</p>
              <p className="text-xl font-bold">{value} <span className="text-sm font-normal text-muted-foreground">{unit}</span></p>
          </div>
      </div>
  );

  return (
    <ScrollArea className="h-full">
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-4 p-1 pt-2">
        
        <Card className="lg:col-span-3">
          <CardHeader>
            <CardTitle className="flex items-center gap-2"><MapPin /> Survey Location &amp; Details</CardTitle>
            <CardDescription>
                Location and environmental data from file
            </CardDescription>
          </CardHeader>
          <CardContent>
            {!apiKey && (
              <div className="h-48 w-full bg-destructive/10 rounded-lg flex flex-col items-center justify-center p-4 text-center text-destructive">
                <AlertTriangle className="w-8 h-8 mb-2" />
                <h4 className="font-semibold">Google Maps API Key is missing.</h4>
                <p className="text-sm">Please add `NEXT_PUBLIC_GOOGLE_MAPS_API_KEY` to your `.env` file to display the map.</p>
              </div>
            )}
            {apiKey && latestData.latitude && latestData.longitude ? (
              <div className="h-48 w-full bg-muted rounded-lg flex items-center justify-center relative overflow-hidden">
                <iframe
                  className="w-full h-full border-0 rounded-lg"
                  loading="lazy"
                  allowFullScreen
                  src={`https://www.google.com/maps/embed/v1/place?key=${apiKey}&q=${latestData.latitude},${latestData.longitude}&zoom=14`}
                >
                </iframe>
              </div>
            ) : (
             apiKey && <div className="h-48 w-full bg-muted rounded-lg flex items-center justify-center relative overflow-hidden">
                <p>No location data available.</p>
              </div>
            )}
            <div className="grid grid-cols-2 md:grid-cols-4 gap-4 pt-4 text-sm">
                <div className="flex items-start gap-2">
                    <Calendar className="w-4 h-4 mt-1 text-muted-foreground" />
                    <div>
                        <p className="font-semibold">Survey Date</p>
                        <p className="text-muted-foreground">{latestData.surveyDate || 'N/A'}</p>
                    </div>
                </div>
                 <div className="flex items-start gap-2">
                    <Clock className="w-4 h-4 mt-1 text-muted-foreground" />
                    <div>
                        <p className="font-semibold">Finish Time</p>
                        <p className="text-muted-foreground">{latestData.surveyFinishTime || 'N/A'}</p>
                    </div>
                </div>
                <div className="flex items-start gap-2">
                    <Thermometer className="w-4 h-4 mt-1 text-muted-foreground" />
                    <div>
                        <p className="font-semibold">Avg. Temp</p>
                        <p className="text-muted-foreground">{avgTemperature}°C</p>
                    </div>
                </div>
                <div className="flex items-start gap-2">
                    <Send className="w-4 h-4 mt-1 text-muted-foreground" />
                    <div>
                        <p className="font-semibold">Fly-bys</p>
                        <p className="text-muted-foreground">{totalFlybys || 0}</p>
                    </div>
                </div>
            </div>
          </CardContent>
        </Card>
        
        <Card className="lg:col-span-1">
            <CardHeader>
                <CardTitle className="flex items-center gap-2 text-lg"><HardDrive /> Device Info</CardTitle>
            </CardHeader>
            <CardContent className="space-y-4">
                 <StatCard icon={<RadioTower/>} label="Model" value={latestData.model || 'N/A'} />
                 <StatCard icon={<GitCommitHorizontal/>} label="Make" value={latestData.make || 'N/A'} />
                 <StatCard icon={<Settings/>} label="Serial" value={latestData.serial || 'N/A'} />
                 <StatCard icon={<Gauge/>} label="Firmware" value={latestData.firmwareVersion || 'N/A'} />
            </CardContent>
        </Card>

        <Card className="lg:col-span-2">
            <CardHeader>
                <CardTitle className="flex items-center gap-2 text-lg"><SlidersHorizontal /> Recording Settings</CardTitle>
            </CardHeader>
            <CardContent className="grid grid-cols-2 gap-x-6 gap-y-4">
                 <StatCard icon={<RadioTower/>} label="Sample Rate" value={formatHz(latestData.sampleRate || 0)} />
                 <StatCard icon={<Zap/>} label="Gain" value={latestData.gain ?? 'N/A'} unit="dB" />
                 
                 <div className="col-span-2"> <Separator /> </div>
                 
                 <p className="col-span-2 text-sm font-medium text-muted-foreground -mb-2">Trigger Details</p>
                 <StatCard icon={<Clock/>} label="Window" value={latestData.triggerWindow ?? 'N/A'} unit="s" />
                 <StatCard icon={<Clock/>} label="Max Length" value={latestData.triggerMaxLen ?? 'N/A'} unit="s" />
                 <StatCard icon={<Zap/>} label="Min Frequency" value={formatHz(latestData.minTriggerFreq || 0)} />
                 <StatCard icon={<Zap/>} label="Max Frequency" value={formatHz(latestData.maxTriggerFreq || 0)} />
                 <StatCard icon={<Clock/>} label="Min Duration" value={latestData.triggerMinDur ?? 'N/A'} unit="s" />
                 <StatCard icon={<Clock/>} label="Max Duration" value={latestData.triggerMaxDur ?? 'N/A'} unit="s" />
            </CardContent>
        </Card>

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
                <Line type="monotone" dataKey="temperature" stroke="hsl(var(--chart-1))" strokeWidth={2} dot={false} />
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
                  <Bar dataKey="flybys" fill="hsl(var(--chart-2))" />
                </BarChart>
              </ResponsiveContainer>
            </CardContent>
          </Card>
        )}

      </div>
    </ScrollArea>
  );
}
