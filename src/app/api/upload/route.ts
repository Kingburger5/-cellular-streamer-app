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

export async function POST(request: NextRequest) {
  try {
    await ensureUploadDirExists();
    console.log("[SERVER] Received upload request.");

    // Read metadata from custom headers (lowercase)
    const fileIdentifier = request.headers.get("x-file-id");
    const chunkIndexStr = request.headers.get("x-chunk-index");
    const totalChunksStr = request.headers.get("x-total-chunks");
    const originalFilename = request.headers.get("x-original-filename");

    console.log(`[SERVER] Headers: x-file-id=${fileIdentifier}, x-chunk-index=${chunkIndexStr}, x-total-chunks=${totalChunksStr}, x-original-filename=${originalFilename}`);

    if (!fileIdentifier || !chunkIndexStr || !totalChunksStr || !originalFilename) {
      console.error("[SERVER] Missing required headers.");
      return NextResponse.json({ error: "Missing required headers." }, { status: 400 });
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
