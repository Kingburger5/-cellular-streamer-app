import { File, FileJson, FileText, Table, Music } from "lucide-react";
import type { ComponentProps } from "react";

interface FileIconProps extends ComponentProps<"svg"> {
  filename: string;
}

export function FileIcon({ filename, ...props }: FileIconProps) {
  const extension = filename.slice(filename.lastIndexOf("."));

  switch (extension) {
    case ".json":
      return <FileJson {...props} />;
    case ".csv":
      return <Table {...props} />;
    case ".txt":
      return <FileText {...props} />;
    case ".wav":
      return <Music {...props} />;
    default:
      return <File {...props} />;
  }
}
