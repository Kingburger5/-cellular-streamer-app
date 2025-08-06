
"use client";

import type { DataPoint } from '@/lib/types';
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from '@/components/ui/card';
import { ScrollArea } from '@/components/ui/scroll-area';
import { BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer, LineChart, Line } from 'recharts';
import { Satellite, Thermometer, Send, FileWarning, AlertTriangle, RadioTower, Zap, HardDrive, Loader, MapPin, Calendar, Clock } from 'lucide-react';
import { formatBytes } from '@/lib/utils';

export function DataVisualizer({ data, isLoading }: { data: DataPoint[] | null, isLoading: boolean }) {
  const apiKey = process.env.NEXT_PUBLIC_GOOGLE_MAPS_API_KEY;

  if (isLoading) {
    return (
        <div className="flex flex-col items-center justify-center h-full text-muted-foreground p-4 text-center">
            <Loader className="w-12 h-12 mb-4 animate-spin" />
            <h3 className="font-semibold">Analyzing Data...</h3>
            <p className="text-sm">The AI is extracting structured data and updating your records.</p>
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
    if (!hz) return 'N/A';
    if (hz >= 1000) {
      return `${(hz / 1000).toFixed(1)} kHz`;
    }
    return `${hz} Hz`;
  }
  
  const latestData = data[0]; // Assuming the most relevant data is the first item

  const avgTemperature = (data.reduce((acc, d) => acc + (d.temperature || 0), 0) / data.length).toFixed(1);
  const totalFlybys = data.reduce((acc, d) => acc + (d.flybys || 0), 0);


  return (
    <ScrollArea className="h-full">
      <div className="grid grid-cols-1 lg:grid-cols-4 gap-4 p-1">
        
        <Card className="lg:col-span-3">
          <CardHeader>
            <CardTitle className="flex items-center gap-2"><MapPin /> Survey Location &amp; Details</CardTitle>
            <CardDescription>
                Location data from file
            </CardDescription>
          </CardHeader>
          <CardContent>
            {!apiKey && (
              <div className="h-64 w-full bg-destructive/10 rounded-lg flex flex-col items-center justify-center p-4 text-center text-destructive">
                <AlertTriangle className="w-8 h-8 mb-2" />
                <h4 className="font-semibold">Google Maps API Key is missing.</h4>
                <p className="text-sm">Please add `NEXT_PUBLIC_GOOGLE_MAPS_API_KEY` to your `.env` file to display the map.</p>
              </div>
            )}
            {apiKey && latestData.latitude && latestData.longitude ? (
              <div className="h-64 w-full bg-muted rounded-lg flex items-center justify-center relative overflow-hidden">
                <iframe
                  className="w-full h-full border-0 rounded-lg"
                  loading="lazy"
                  allowFullScreen
                  src={`https://www.google.com/maps/embed/v1/place?key=${apiKey}&q=${latestData.latitude},${latestData.longitude}&zoom=14`}
                >
                </iframe>
              </div>
            ) : (
             apiKey && <div className="h-64 w-full bg-muted rounded-lg flex items-center justify-center relative overflow-hidden">
                <p>No location data available.</p>
              </div>
            )}
            <div className="grid grid-cols-2 gap-4 pt-4 text-sm">
                <div className="flex items-start gap-2">
                    <Calendar className="w-4 h-4 mt-1 text-muted-foreground" />
                    <div>
                        <p className="font-semibold">Survey Date</p>
                        <p className="text-muted-foreground">{latestData.surveyDate || 'Not Available'}</p>
                    </div>
                </div>
                 <div className="flex items-start gap-2">
                    <Clock className="w-4 h-4 mt-1 text-muted-foreground" />
                    <div>
                        <p className="font-semibold">Finish Time</p>
                        <p className="text-muted-foreground">{latestData.surveyFinishTime || 'Not Available'}</p>
                    </div>
                </div>
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
            {latestData?.make && latestData?.model && (
                 <Card>
                    <CardHeader className="pb-2">
                        <CardTitle className="text-base font-normal flex items-center gap-2"><HardDrive /> Device</CardTitle>
                    </CardHeader>
                    <CardContent>
                        <p className="text-lg font-bold leading-tight">{latestData.model}</p>
                        <p className="text-sm text-muted-foreground">{latestData.make}</p>
                        {latestData.serial && <p className="text-xs text-muted-foreground mt-1">S/N: {latestData.serial}</p>}
                    </CardContent>
                </Card>
            )}
        </div>

        {latestData.sampleRate && (
            <Card className="lg:col-span-2">
                <CardHeader className="pb-2">
                    <CardTitle className="text-base font-normal flex items-center gap-2"><RadioTower /> Sample Rate</CardTitle>
                </CardHeader>
                <CardContent>
                    <p className="text-3xl font-bold">{formatHz(latestData.sampleRate)}</p>
                </CardContent>
            </Card>
        )}

        {(latestData.minTriggerFreq || latestData.maxTriggerFreq) && (
             <Card className="lg:col-span-2">
                <CardHeader className="pb-2">
                    <CardTitle className="text-base font-normal flex items-center gap-2"><Zap /> Trigger Frequency</CardTitle>
                </CardHeader>
                <CardContent>
                    <div className="flex justify-around items-center pt-2">
                        {latestData.minTriggerFreq && <div className="text-center"><p className="text-xs text-muted-foreground">Min</p><p className="text-2xl font-bold">{formatHz(latestData.minTriggerFreq)}</p></div>}
                        {latestData.maxTriggerFreq && <div className="text-center"><p className="text-xs text-muted-foreground">Max</p><p className="text-2xl font-bold">{formatHz(latestData.maxTriggerFreq)}</p></div>}
                    </div>
                </CardContent>
            </Card>
        )}

        <Card className="lg:col-span-4">
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
          <Card className="lg:col-span-4">
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
