
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

  // Original fields
  timestamp: string;
  latitude: number;
  longitude: number;
  temperature: number;
  length?: number; // Added to calculate finish time
  flybys?: number;
  sampleRate?: number;
  minTriggerFreq?: number;
  maxTriggerFreq?: number;
  make?: string;
  model?: string;
  serial?: string;
}

export interface FileContent {
  content: string;
  extension: string;
  name: string;
  isBinary?: boolean;
  extractedData?: DataPoint[] | null;
  rawMetadata?: string | null;
}

export interface ServerHealth {
    canInitializeAdmin: boolean;
    hasProjectId: boolean;
    hasClientEmail: boolean;
    hasPrivateKey: boolean;
    canFetchAccessToken: boolean;
    accessTokenError: string | null;
    projectId: string | null;
    detectedClientEmail: string | null;
}
