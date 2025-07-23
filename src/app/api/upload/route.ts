import { NextRequest, NextResponse } from "next/server";
import fs from "fs/promises";
import path from "path";

const UPLOAD_DIR = "/tmp/uploads";
const CHUNK_DIR = "/tmp/uploads/chunks";

async function ensureDirExists(dir: string) {
  try {
    await fs.access(dir);
  } catch {
    await fs.mkdir(dir, { recursive: true });
  }
}

async function reassembleFile(fileIdentifier: string, originalFilename: string) {
  const chunkDir = path.join(CHUNK_DIR, fileIdentifier);
  // Sanitize the original filename before using it
  const sanitizedOriginalFilename = path.basename(originalFilename);
  const finalFilePath = path.join(UPLOAD_DIR, `${Date.now()}-${sanitizedOriginalFilename}`);

  try {
    const chunkFilenames = (await fs.readdir(chunkDir)).sort((a, b) => parseInt(a) - parseInt(b));
    
    // Use file handle for efficient writing
    const finalFileWriteStream = await fs.open(finalFilePath, 'w');

    for (const chunkFilename of chunkFilenames) {
      const chunkPath = path.join(chunkDir, chunkFilename);
      const chunkBuffer = await fs.readFile(chunkPath);
      await fs.writeFile(finalFileWriteStream, chunkBuffer);
      await fs.unlink(chunkPath); // Clean up chunk
    }

    await finalFileWriteStream.close();
    await fs.rmdir(chunkDir); // Clean up chunk directory

    return finalFilePath;
  } catch (error) {
    console.error("Error reassembling file:", error);
    // Cleanup on error
    try {
      await fs.rm(chunkDir, { recursive: true, force: true });
    } catch (cleanupError) {
      console.error("Error during cleanup:", cleanupError);
    }
    throw new Error("Failed to reassemble file.");
  }
}

export async function POST(request: NextRequest) {
  try {
    const filename = request.headers.get("x-filename");
    const chunkOffsetHeader = request.headers.get("x-chunk-offset");
    const chunkSizeHeader = request.headers.get("x-chunk-size");
    const totalSizeHeader = request.headers.get("x-total-size");

    // The body is now raw binary data, not form data
    const chunkBuffer = Buffer.from(await request.arrayBuffer());

    const chunkOffset = chunkOffsetHeader ? parseInt(chunkOffsetHeader, 10) : NaN;
    const chunkSize = chunkSizeHeader ? parseInt(chunkSizeHeader, 10) : NaN;
    const totalSize = totalSizeHeader ? parseInt(totalSizeHeader, 10) : NaN;

    if (!filename || isNaN(chunkOffset) || isNaN(chunkSize) || isNaN(totalSize) || chunkBuffer.length === 0) {
      return NextResponse.json({ error: "Missing or invalid required headers or empty chunk." }, { status: 400 });
    }

    // Sanitize filename to prevent directory traversal
    const sanitizedFilename = path.basename(filename);
    const fileIdentifier = `${sanitizedFilename.replace(/[^a-zA-Z0-9.-]/g, '_')}-${totalSize}`;
    
    const chunkDir = path.join(CHUNK_DIR, fileIdentifier);
    await ensureDirExists(chunkDir);

    const chunkIndex = Math.floor(chunkOffset / chunkSize);
    const chunkPath = path.join(chunkDir, chunkIndex.toString());
    
    await fs.writeFile(chunkPath, chunkBuffer);

    // Check if all chunks have been received by counting them
    const existingChunks = await fs.readdir(chunkDir);
    const totalChunks = Math.ceil(totalSize / chunkSize);
    const isLastChunk = existingChunks.length === totalChunks;

    if (isLastChunk) {
        // Ensure all chunks are actually on disk before reassembly
        if (existingChunks.length * chunkSize >= totalSize || Math.abs(existingChunks.length - totalChunks) <=1) {
             const finalPath = await reassembleFile(fileIdentifier, sanitizedFilename);
             console.log(`File reassembled successfully: ${path.basename(finalPath)}`);
             return NextResponse.json({ message: "File reassembled successfully.", filename: path.basename(finalPath) }, { status: 200 }); // Changed to 200 for OK
        }
    }

    return NextResponse.json({ message: `Chunk ${chunkIndex + 1}/${totalChunks} received.` }, { status: 200 });
  } catch (error) {
    console.error("Upload error:", error);
    const errorMessage = error instanceof Error ? error.message : "Internal Server Error";
    return NextResponse.json(
      { error: "Internal Server Error", details: errorMessage },
      { status: 500 }
    );
  }
}
