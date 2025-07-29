
import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";

const UPLOAD_DIR = path.join(process.cwd(), "uploads");

async function ensureUploadDirExists() {
  try {
    await fs.access(UPLOAD_DIR);
  } catch {
    await fs.mkdir(UPLOAD_DIR, { recursive: true });
  }
}

function parseUserDataHeader(header: string): Record<string, string> {
    const result: Record<string, string> = {};
    if (!header) return result;

    const pairs = header.split(';');
    pairs.forEach(pair => {
        const parts = pair.split(':');
        if (parts.length === 2) {
            const key = parts[0].trim().toLowerCase();
            const value = parts[1].trim();
            result[key] = value;
        }
    });
    return result;
}

export async function POST(request: NextRequest) {
  try {
    await ensureUploadDirExists();
    console.log("[SERVER] Received upload request.");
    
    // Read the custom semicolon-delimited header string
    const userData = request.headers.get("userdata") || request.headers.get("USERDATA");
    console.log(`[SERVER] Raw USERDATA header: ${userData}`);

    if (!userData) {
        return NextResponse.json({ error: "Missing USERDATA header." }, { status: 400 });
    }
    
    // Parse the custom header string
    const headers = parseUserDataHeader(userData);
    
    const fileIdentifier = headers["x-file-id"];
    const chunkIndexStr = headers["x-chunk-index"];
    const totalChunksStr = headers["x-total-chunks"];
    const originalFilename = headers["x-original-filename"];


    console.log(`[SERVER] Parsed Headers: x-file-id=${fileIdentifier}, x-chunk-index=${chunkIndexStr}, x-total-chunks=${totalChunksStr}, x-original-filename=${originalFilename}`);

    if (!fileIdentifier || !chunkIndexStr || !totalChunksStr || !originalFilename) {
      console.error("[SERVER] Missing required fields in USERDATA header.");
      return NextResponse.json({ error: "Missing required fields in USERDATA header." }, { status: 400 });
    }

    const chunkIndex = parseInt(chunkIndexStr);
    const totalChunks = parseInt(totalChunksStr);

    // Read raw binary data from the request body
    const chunkBuffer = await request.arrayBuffer();
    if (!chunkBuffer || chunkBuffer.byteLength === 0) {
      console.error("[SERVER] Empty chunk received.");
      return NextResponse.json({ error: "Empty chunk received." }, { status: 400 });
    }
    const buffer = Buffer.from(chunkBuffer);
    console.log(`[SERVER] Received chunk ${chunkIndex + 1}/${totalChunks} with size ${buffer.length} bytes.`);


    // Sanitize filename to prevent directory traversal
    const safeFilename = path.basename(originalFilename);
    const tempFilePath = path.join(UPLOAD_DIR, `${fileIdentifier}.part`);

    await fs.appendFile(tempFilePath, buffer);
    console.log(`[SERVER] Appended chunk ${chunkIndex + 1} to ${tempFilePath}`);

    if (chunkIndex === totalChunks - 1) {
      // Last chunk, rename the file
      const finalFilePath = path.join(UPLOAD_DIR, safeFilename);
      try {
        await fs.access(finalFilePath);
        console.warn(`[SERVER] File ${finalFilePath} already exists. Overwriting.`);
        await fs.unlink(finalFilePath);
      } catch {
        // File doesn't exist, which is fine
      }
      await fs.rename(tempFilePath, finalFilePath);
      console.log(`[SERVER] File upload completed. Renamed ${tempFilePath} to ${finalFilePath}`);
      return NextResponse.json({ message: "File uploaded successfully.", filename: safeFilename }, { status: 200 });
    }

    return NextResponse.json({ message: `Chunk ${chunkIndex + 1}/${totalChunks} received.` }, { status: 200 });

  } catch (error) {
    console.error("[SERVER] Upload error:", error);
    const errorMessage = error instanceof Error ? error.message : "Internal Server Error";
    return NextResponse.json(
      { error: "Internal Server Error", details: errorMessage },
      { status: 500 }
    );
  }
}
