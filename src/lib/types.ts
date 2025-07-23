
export interface UploadedFile {
  name: string;
  size: number;
  uploadDate: Date;
}

export interface DataPoint {
  timestamp: string;
  latitude: number;
  longitude: number;
  temperature: number;
  flybys?: number;
  sampleRate?: number;
  minTriggerFreq?: number;
  maxTriggerFreq?: number;
  make?: string;
  model?: string;
}

export interface FileContent {
  content: string;
  extension: string;
  name: string;
  isBinary?: boolean;
  extractedData?: DataPoint[] | null;
  rawMetadata?: string | null;
}
