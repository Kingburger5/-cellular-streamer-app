 'use server';

/**
 * @fileOverview An AI agent for extracting structured data from GUANO metadata text.
 *
 * - extractData - A function that extracts structured data from a text blob.
 * - ExtractDataInput - The input type for the extractData function.
 * - ExtractDataOutput - The return type for the extractData function.
 */

import {ai} from '@/ai/genkit';
import {z} from 'zod';
import type { DataPoint } from '@/lib/types';


const ExtractDataInputSchema = z.object({
  fileContent: z.string().describe('The GUANO metadata text content to extract data from.'),
  filename: z.string().describe('The original filename of the uploaded file.'),
});
export type ExtractDataInput = z.infer<typeof ExtractDataInputSchema>;


// This schema is designed to exactly match the desired nested JSON output.
const ExtractDataOutputSchema = z.object({
  data: z.array(z.object({
    fileInformation: z.object({
      originalFilename: z.string().optional(),
      recordingDateTime: z.string().optional(),
      recordingDurationSeconds: z.number().optional(),
      sampleRateHz: z.number().optional(),
    }),
    recorderDetails: z.object({
      make: z.string().optional(),
      model: z.string().optional(),
      serialNumber: z.string().optional(),
      firmwareVersion: z.string().optional(),
      gainSetting: z.number().optional(),
    }),
    locationEnvironmentalData: z.object({
      latitude: z.number().optional(),
      longitude: z.number().optional(),
      temperatureCelsius: z.number().optional(),
    }),
    triggerSettings: z.object({
      windowSeconds: z.number().optional(),
      maxLengthSeconds: z.number().optional(),
      minFrequencyHz: z.number().optional(),
      maxFrequencyHz: z.number().optional(),
      minDurationSeconds: z.number().optional(),
      maxDurationSeconds: z.number().optional(),
    }),
  })).describe('The extracted structured data points.'),
});
export type ExtractDataOutput = z.infer<typeof ExtractDataOutputSchema>;


// The wrapper function is now simplified to just call the AI flow.
export async function extractData(input: ExtractDataInput): Promise<ExtractDataOutput> {
  return extractDataFlow(input);
}


const prompt = ai.definePrompt({
  name: 'extractDataPrompt',
  input: {schema: ExtractDataInputSchema},
  output: {schema: ExtractDataOutputSchema},
  prompt: `You are an expert at parsing GUANO metadata text into a structured JSON object.

**CRITICAL INSTRUCTIONS:**
1.  Parse the provided text and extract the data into the JSON structure defined by the output schema.
2.  **Audio settings**: This is a JSON string inside a single-element array (e.g., '[{...}]'). You MUST parse this JSON to get the trigger settings and gain. The key names in the JSON map directly to the schema (e.g., "trig window" -> "windowSeconds").
3.  **Loc Position**: This field contains Latitude and Longitude as two numbers separated by a space. You MUST extract the first number as 'latitude' and the second as 'longitude'.
4.  **Field Mapping**: Map the GUANO fields to the JSON output fields precisely as shown in the example.
5.  **Omissions**: If a field is not present in the text, you MUST omit it from the output object. Do not guess or fill with default values.
6.  **Calculations**: Do not perform any calculations or conversions (e.g., Hz to kHz). Return the raw numeric values as they appear in the source text.
7.  **Output**: You must return a single object in the 'data' array. If no data can be parsed, return an empty array.

**EXAMPLE:**

**Input Text:**
GUANO|Version:1.0|Firmware Version:4.6|Make:Wildlife Acoustics, Inc.|Model:Song Meter Mini Bat|Serial:SMU06612|WA|Song Meter|Prefix:1|WA|Song Meter|Audio settings:[{"rate":256000,"gain":12,"trig window":3.0,"trig max len":15.0,"trig min freq":30000,"trig max freq":128000,"trig min dur":0.0015,"trig max dur":0.0000}]|Length:3.921|Original Filename:1_20250302_205009.wav|Timestamp:2025-03-02 20:50:09+13:00|Loc Position:-37.00403 174.57577|Temperature Int:20.75|Samplerate:256000

**Your JSON output for this example MUST BE:**
\`\`\`json
{
  "data": [
    {
      "fileInformation": {
        "originalFilename": "1_20250302_205009.wav",
        "recordingDateTime": "2025-03-02 20:50:09+13:00",
        "recordingDurationSeconds": 3.921,
        "sampleRateHz": 256000
      },
      "recorderDetails": {
        "make": "Wildlife Acoustics, Inc.",
        "model": "Song Meter Mini Bat",
        "serialNumber": "SMU06612",
        "firmwareVersion": "4.6",
        "gainSetting": 12
      },
      "locationEnvironmentalData": {
        "latitude": -37.00403,
        "longitude": 174.57577,
        "temperatureCelsius": 20.75
      },
      "triggerSettings": {
        "windowSeconds": 3.0,
        "maxLengthSeconds": 15.0,
        "minFrequencyHz": 30000,
        "maxFrequencyHz": 128000,
        "minDurationSeconds": 0.0015,
        "maxDurationSeconds": 0.0
      }
    }
  ]
}
\`\`\`

Now, please process the following content following all rules precisely.

Filename: {{{filename}}}
Content:
{{{fileContent}}}
`,
});

const extractDataFlow = ai.defineFlow(
  {
    name: 'extractDataFlow',
    inputSchema: ExtractDataInputSchema,
    outputSchema: ExtractDataOutputSchema,
  },
  async (input) => {
    const {output} = await prompt(input);
    return output!;
  }
);
