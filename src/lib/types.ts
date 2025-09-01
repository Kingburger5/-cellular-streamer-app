
export interface UploadedFile {
  name: string;
  size: number;
  uploadDate: Date;
}

// This represents the fully parsed, nested data structure from the AI.
export interface DataPoint {
  fileInformation: {
    originalFilename?: string;
    recordingDateTime?: string;
    recordingDurationSeconds?: number;
    sampleRateHz?: number;
  };
  recorderDetails: {
    make?: string;
    model?: string;
    serialNumber?: string;
    firmwareVersion?: string;
    gainSetting?: number;
  };
  locationEnvironmentalData: {
    latitude?: number;
    longitude?: number;
    temperatureCelsius?: number;
  };
  triggerSettings: {
    windowSeconds?: number;
    maxLengthSeconds?: number;
    minFrequencyHz?: number;
    maxFrequencyHz?: number;
    minDurationSeconds?: number;
    maxDurationSeconds?: number;
  };
}

export interface FileContent {
  content: string;
  extension: string;
  name: string;
  isBinary?: boolean;
  // This now holds an array of the structured DataPoint objects.
  extractedData?: DataPoint[] | null;
  rawMetadata?: string | null;
}
