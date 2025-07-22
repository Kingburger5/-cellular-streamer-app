export interface UploadedFile {
  name: string;
  size: number;
  uploadDate: Date;
}

export interface FileContent {
  content: string;
  extension: string;
  name: string;
  isBinary?: boolean;
}
