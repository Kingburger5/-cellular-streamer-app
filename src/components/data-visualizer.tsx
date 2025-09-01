
"use client";

import type { DataPoint } from '@/lib/types';
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from '@/components/ui/card';
import { ScrollArea } from '@/components/ui/scroll-area';
import { Thermometer, RadioTower, HardDrive, Loader, MapPin, Calendar, Clock, Gauge, Settings, SlidersHorizontal, GitCommitHorizontal, Zap, AlertTriangle, FileWarning } from 'lucide-react';
import { Separator } from '@/components/ui/separator';

export function DataVisualizer({ data, fileName, isLoading }: { data: DataPoint[] | null, fileName: string, isLoading: boolean }) {
  const apiKey = process.env.NEXT_PUBLIC_GOOGLE_MAPS_API_KEY;
  
  if (isLoading) {
    return (
        <div className="flex flex-col items-center justify-center h-full text-muted-foreground p-4 text-center">
            <Loader className="w-12 h-12 mb-4 animate-spin" />
            <h3 className="font-semibold">Analyzing Data...</h3>
            <p className="text-sm">AI is extracting structured data from the file's metadata.</p>
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

  const formatHz = (hz: number | undefined) => {
    if (!hz && hz !== 0) return 'N/A';
    if (hz >= 1000) {
      return `${(hz / 1000).toFixed(1)} kHz`;
    }
    return `${hz} Hz`;
  }
  
  // Use the first data point as the source of truth for display
  const latestData = data[0];
  const { fileInformation, recorderDetails, locationEnvironmentalData, triggerSettings } = latestData;

  const StatCard = ({ icon, label, value, unit }: { icon: React.ReactNode, label: string, value: string | number | undefined, unit?: string}) => (
      <div className="flex items-center gap-4">
          <div className="bg-muted p-2 rounded-lg">
              {icon}
          </div>
          <div>
              <p className="text-sm text-muted-foreground">{label}</p>
              <p className="text-xl font-bold">{value ?? 'N/A'}{value !== undefined && unit ? <span className="text-sm font-normal text-muted-foreground ml-1">{unit}</span> : ''}</p>
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
                Location and environmental data extracted from the file.
            </CardDescription>
          </CardHeader>
          <CardContent>
            {!apiKey || apiKey === "YOUR_API_KEY_HERE" ? (
              <div className="h-48 w-full bg-destructive/10 rounded-lg flex flex-col items-center justify-center p-4 text-center text-destructive">
                <AlertTriangle className="w-8 h-8 mb-2" />
                <h4 className="font-semibold">Google Maps API Key is missing.</h4>
                <p className="text-sm">Please add `NEXT_PUBLIC_GOOGLE_MAPS_API_KEY` to your `.env` file to display the map.</p>
              </div>
            ) : locationEnvironmentalData.latitude && locationEnvironmentalData.longitude ? (
              <div className="h-48 w-full bg-muted rounded-lg flex items-center justify-center relative overflow-hidden">
                <iframe
                  className="w-full h-full border-0 rounded-lg"
                  loading="lazy"
                  allowFullScreen
                  src={`https://www.google.com/maps/embed/v1/place?key=${apiKey}&q=${locationEnvironmentalData.latitude},${locationEnvironmentalData.longitude}&zoom=14`}
                >
                </iframe>
              </div>
            ) : (
             <div className="h-48 w-full bg-muted rounded-lg flex items-center justify-center relative overflow-hidden">
                <p>No location data available.</p>
              </div>
            )}
            <div className="grid grid-cols-2 md:grid-cols-4 gap-4 pt-4 text-sm">
                <div className="flex items-start gap-2">
                    <Calendar className="w-4 h-4 mt-1 text-muted-foreground" />
                    <div>
                        <p className="font-semibold">Recording Date</p>
                        <p className="text-muted-foreground">{fileInformation.recordingDateTime ? new Date(fileInformation.recordingDateTime).toLocaleString() : 'N/A'}</p>
                    </div>
                </div>
                 <div className="flex items-start gap-2">
                    <Clock className="w-4 h-4 mt-1 text-muted-foreground" />
                    <div>
                        <p className="font-semibold">Duration</p>
                        <p className="text-muted-foreground">{fileInformation.recordingDurationSeconds} s</p>
                    </div>
                </div>
                <div className="flex items-start gap-2">
                    <Thermometer className="w-4 h-4 mt-1 text-muted-foreground" />
                    <div>
                        <p className="font-semibold">Temperature</p>
                        <p className="text-muted-foreground">{locationEnvironmentalData.temperatureCelsius}°C</p>
                    </div>
                </div>
                 <div className="flex items-start gap-2">
                    <RadioTower className="w-4 h-4 mt-1 text-muted-foreground" />
                    <div>
                        <p className="font-semibold">Sample Rate</p>
                        <p className="text-muted-foreground">{formatHz(fileInformation.sampleRateHz)}</p>
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
                 <StatCard icon={<RadioTower/>} label="Model" value={recorderDetails.model} />
                 <StatCard icon={<GitCommitHorizontal/>} label="Make" value={recorderDetails.make} />
                 <StatCard icon={<Settings/>} label="Serial" value={recorderDetails.serialNumber} />
                 <StatCard icon={<Gauge/>} label="Firmware" value={recorderDetails.firmwareVersion} />
            </CardContent>
        </Card>

        <Card className="lg:col-span-2">
            <CardHeader>
                <CardTitle className="flex items-center gap-2 text-lg"><SlidersHorizontal /> Recording Settings</CardTitle>
            </CardHeader>
            <CardContent className="grid grid-cols-2 gap-x-6 gap-y-4">
                 <StatCard icon={<Zap/>} label="Gain" value={recorderDetails.gainSetting} unit="dB" />
                 <div></div>

                 <div className="col-span-2"> <Separator /> </div>
                 
                 <p className="col-span-2 text-sm font-medium text-muted-foreground -mb-2">Trigger Details</p>
                 <StatCard icon={<Clock/>} label="Window" value={triggerSettings.windowSeconds?.toFixed(1)} unit="s" />
                 <StatCard icon={<Clock/>} label="Max Length" value={triggerSettings.maxLengthSeconds} unit="s" />
                 <StatCard icon={<Zap/>} label="Min Frequency" value={formatHz(triggerSettings.minFrequencyHz)} />
                 <StatCard icon={<Zap/>} label="Max Frequency" value={formatHz(triggerSettings.maxFrequencyHz)} />
                 <StatCard icon={<Clock/>} label="Min Duration" value={triggerSettings.minDurationSeconds} unit="s" />
                 <StatCard icon={<Clock/>} label="Max Duration" value={triggerSettings.maxDurationSeconds} unit="s" />
            </CardContent>
        </Card>

      </div>
    </ScrollArea>
  );
}
