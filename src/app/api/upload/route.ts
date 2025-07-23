import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";

const UPLOAD_DIR = "/tmp/uploads";

async function ensureDirExists(dir: string) {
  try {
    await fs.access(dir);
  } catch {
    await fs.mkdir(dir, { recursive: true });
  }
}

export async function POST(request: NextRequest) {
  try {
    await ensureDirExists(UPLOAD_DIR);

    const filename = request.headers.get("x-filename");
    if (!filename) {
      return NextResponse.json({ error: "x-filename header is required." }, { status: 400 });
    }
    
    // Sanitize the original filename before using it
    const sanitizedOriginalFilename = path.basename(filename);
    const finalFilePath = path.join(UPLOAD_DIR, `${Date.now()}-${sanitizedOriginalFilename}`);

    const bodyBuffer = Buffer.from(await request.arrayBuffer());
    
    await fs.writeFile(finalFilePath, bodyBuffer);

    console.log(`File uploaded successfully: ${path.basename(finalFilePath)}`);
    return NextResponse.json({ message: "File uploaded successfully.", filename: path.basename(finalFilePath) }, { status: 200 });
    
  } catch (error) {
    console.error("Upload error:", error);
    const errorMessage = error instanceof Error ? error.message : "Internal Server Error";
    return NextResponse.json(
      { error: "Internal Server Error", details: errorMessage },
      { status: 500 }
    );
  }
}
