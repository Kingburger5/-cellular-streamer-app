
    import { NextRequest, NextResponse } from "next/server";
    import { getAdminStorage } from "@/lib/firebase-admin";
    import { promises as fs } from 'fs';
    import path from 'path';
    import os from 'os';

    const BUCKET_NAME = "cellular-data-streamer.firebasestorage.app";

    // Ensure the temporary directory exists
    const TMP_DIR = path.join(os.tmpdir(), 'uploads');
    const ensureTmpDir = async () => {
        try {
            await fs.mkdir(TMP_DIR, { recursive: true });
        } catch (error) {
            console.error("Failed to create temporary directory:", error);
        }
    };
    ensureTmpDir();


    /**
     * Acts as a secure relay for file uploads from SIM7600 modules.
     * It accepts the raw file in the request body and uses the Firebase Admin SDK
     * to save it to a private Firebase Storage bucket.
     * The original filename is expected in the 'x-original-filename' header.
     * It now supports chunked uploads.
     */
    export async function POST(request: NextRequest) {
        console.log(`[SERVER] Received new POST request to /api/upload.`);
        try {
            // Get headers for chunking
            const originalFilename = request.headers.get('x-original-filename');
            const chunkIndexStr = request.headers.get('x-chunk-index');
            const totalChunksStr = request.headers.get('x-total-chunks');

            console.log("[SERVER] Headers:", request.headers);


            if (!originalFilename) {
                console.error("[SERVER_ERROR] Missing 'x-original-filename' header.");
                return NextResponse.json({ error: "Missing 'x-original-filename' header." }, { status: 400 });
            }
            
            const isChunked = chunkIndexStr !== null && totalChunksStr !== null;

            if (isChunked) {
                const chunkIndex = parseInt(chunkIndexStr!, 10);
                const totalChunks = parseInt(totalChunksStr!, 10);
                 console.log(`[SERVER] Processing chunk ${chunkIndex + 1} of ${totalChunks} for ${originalFilename}.`);
                
                if (isNaN(chunkIndex) || isNaN(totalChunks)) {
                    console.error("[SERVER_ERROR] Invalid chunking headers.");
                    return NextResponse.json({ error: "Invalid chunking headers." }, { status: 400 });
                }
                
                // Save chunk to a temporary file
                const chunkBuffer = await request.arrayBuffer();
                console.log("[SERVER] Chunk body size:", chunkBuffer.byteLength, "bytes");

                if (chunkBuffer.byteLength === 0) {
                     console.warn("[SERVER] Received empty chunk.");
                     return NextResponse.json({ error: "Received empty chunk data." }, { status: 400 });
                }

                const chunkFilePath = path.join(TMP_DIR, `${originalFilename}.part_${chunkIndex}`);
                await fs.writeFile(chunkFilePath, Buffer.from(chunkBuffer));
                
                // Check if all chunks are received
                if (chunkIndex === totalChunks - 1) {
                    console.log(`[SERVER] Received final chunk for ${originalFilename}. Assembling file.`);
                    
                    // Assemble the file
                    const assembledFilePath = path.join(TMP_DIR, originalFilename);
                    const fileWriteStream = await fs.open(assembledFilePath, 'w');

                    for (let i = 0; i < totalChunks; i++) {
                        const partPath = path.join(TMP_DIR, `${originalFilename}.part_${i}`);
                        const partData = await fs.readFile(partPath);
                        await fileWriteStream.write(partData);
                        await fs.unlink(partPath); // Clean up part file
                    }
                    await fileWriteStream.close();
                    
                    const fileBuffer = await fs.readFile(assembledFilePath);
                    
                    // Upload the complete file to Firebase Storage
                    const adminStorage = await getAdminStorage();
                    const bucket = adminStorage.bucket(BUCKET_NAME);
                    const file = bucket.file(`uploads/${originalFilename}`);
                    const contentType = request.headers.get('content-type') || 'application/octet-stream';
                    
                    await file.save(fileBuffer, { metadata: { contentType } });
                    
                    // Clean up the assembled file
                    await fs.unlink(assembledFilePath);

                    console.log(`[SERVER] Relay upload successful. Assembled and saved '${originalFilename}' to Firebase Storage.`);
                    // Notify clients
                    const channel = new BroadcastChannel('new-upload');
                    channel.postMessage({ filename: originalFilename });
                    channel.close();

                    return NextResponse.json({ message: "File assembled and uploaded successfully.", filename: originalFilename }, { status: 200 });

                } else {
                    return NextResponse.json({ message: `Chunk ${chunkIndex + 1}/${totalChunks} received.` }, { status: 202 }); // 202 Accepted
                }

            } else {
                // --- Original logic for non-chunked uploads ---
                console.log(`[SERVER] Processing non-chunked upload for ${originalFilename}.`);
                const fileBuffer = await request.arrayBuffer();
                console.log("[SERVER] Body size:", fileBuffer.byteLength, "bytes");


                if (!fileBuffer || fileBuffer.byteLength === 0) {
                    console.warn("[SERVER] Upload attempt with empty file rejected.");
                    return NextResponse.json({ error: "File content is empty." }, { status: 400 });
                }

                const filePath = `uploads/${originalFilename}`;
                const contentType = request.headers.get('content-type') || 'application/octet-stream';

                // Use the Admin SDK to upload the file buffer to the bucket.
                const adminStorage = await getAdminStorage();
                const bucket = adminStorage.bucket(BUCKET_NAME);
                const file = bucket.file(filePath);

                await file.save(Buffer.from(fileBuffer), {
                    metadata: { contentType }
                });

                console.log(`[SERVER] Relay upload successful. Saved '${originalFilename}' to Firebase Storage.`);

                // Notify any connected clients that a new file has arrived.
                const channel = new BroadcastChannel('new-upload');
                channel.postMessage({ filename: originalFilename });
                channel.close();

                return NextResponse.json({
                    message: "File uploaded successfully via relay.",
                    filename: originalFilename,
                }, { status: 200 });
            }

        } catch (error: any) {
            console.error("[SERVER] Unhandled error in upload relay handler:", error);
            const errorMessage = error instanceof Error ? error.message : "An unknown server error occurred.";
            return NextResponse.json(
                { error: "Internal Server Error", details: errorMessage },
                { status: 500 }
            );
        }
    }
