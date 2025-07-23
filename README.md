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
  const finalFilePath = path.join(UPLOAD_DIR, `${Date.now()}-${originalFilename}`);

  try {
    const chunkFilenames = (await fs.readdir(chunkDir)).sort((a, b) => parseInt(a) - parseInt(b));
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
    const chunkOffset = parseInt(request.headers.get("x-chunk-offset") || "0", 10);
    const chunkSize = parseInt(request.headers.get("x-chunk-size") || "0", 10);
    const totalSize = parseInt(request.headers.get("x-total-size") || "0", 10);

    if (!filename || isNaN(chunkOffset) || isNaN(chunkSize) || isNaN(totalSize)) {
      return NextResponse.json({ error: "Missing or invalid required headers." }, { status: 400 });
    }

    // Use filename and total size as a simple file identifier
    const fileIdentifier = `${filename}-${totalSize}`;
    const chunkIndex = Math.floor(chunkOffset / chunkSize);
    const totalChunks = Math.ceil(totalSize / chunkSize);

    await ensureDirExists(UPLOAD_DIR);
    const chunkDir = path.join(CHUNK_DIR, fileIdentifier);
    await ensureDirExists(chunkDir);

    const chunkPath = path.join(chunkDir, chunkIndex.toString());
    const chunkBuffer = Buffer.from(await request.arrayBuffer()); // Read the raw chunk data

    // Basic validation for chunk size
    if (chunkBuffer.length !== chunkSize) {
      console.warn(`Received chunk size (${chunkBuffer.length}) does not match expected chunk size (${chunkSize}) for chunk ${chunkIndex} of ${filename}`);
      // Depending on your needs, you might want to return an error here
    }

    await fs.writeFile(chunkPath, chunkBuffer);

    const isLastChunk = chunkIndex === totalChunks - 1;
    if (isLastChunk) {
      const finalPath = await reassembleFile(fileIdentifier, filename);
      return NextResponse.json({ message: "File reassembled successfully.", filename: path.basename(finalPath) }, { status: 201 });
    }

    return NextResponse.json({ message: "Chunk uploaded successfully." }, { status: 200 });
  } catch (error) {
    console.error("Upload error:", error);
    return NextResponse.json(
      { error: "Internal Server Error" },
      { status: 500 }
    );
  }
}
# Firebase Studio

This is a NextJS starter in Firebase Studio.

To get started, take a look at src/app/page.tsx.
