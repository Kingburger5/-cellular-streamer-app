
export interface UploadedFile {
  name: string;
  size: number;
  uploadDate: Date;
}

export interface DataPoint {
  // New fields from Google Sheet spec
  siteName?: string;
  surveyDate?: string; // Format: YYYY-MM-DD
  surveyFinishTime?: string; // Format: HH:MM:SS

  // Original fields from GUANO
  timestamp: string;
  latitude: number;
  longitude: number;
  temperature: number;
  length?: number; 
  flybys?: number;
  sampleRate?: number;
  minTriggerFreq?: number;
  maxTriggerFreq?: number;
  make?: string;
  model?: string;
  serial?: string;
  
  // Expanded GUANO fields
  firmwareVersion?: string;
  gain?: number;
  triggerWindow?: number;
  triggerMaxLen?: number;
  triggerMinDur?: number;
  triggerMaxDur?: number;
}

export interface FileContent {
  content: string;
  extension: string;
  name: string;
  isBinary?: boolean;
  extractedData?: DataPoint[] | null;
  rawMetadata?: string | null;
}
