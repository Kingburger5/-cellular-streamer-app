 'use server';

/**
 * @fileOverview An AI agent for extracting structured data from text.
 *
 * - extractData - A function that extracts structured data from a text blob.
 * - ExtractDataInput - The input type for the extractData function.
 * - ExtractDataOutput - The return type for the extractData function.
 */

import {ai} from '@/ai/genkit';
import {z} from 'genkit';

const DataPointSchema = z.object({
  timestamp: z.string().describe('The timestamp of the data point (e.g., "2025-03-02 20:50:16+13:00").'),
  latitude: z.number().describe('The latitude of the location.'),
  longitude: z.number().describe('The longitude of the location.'),
  temperature: z.number().describe('The temperature reading.'),
  flybys: z.number().optional().describe('The number of fly-bys. This field may not be present.'),
});

const ExtractDataInputSchema = z.object({
  fileContent: z.string().describe('The text content to extract data from.'),
});
export type ExtractDataInput = z.infer<typeof ExtractDataInputSchema>;

const ExtractDataOutputSchema = z.object({
  data: z.array(DataPointSchema).describe('The extracted structured data points.'),
});
export type ExtractDataOutput = z.infer<typeof ExtractDataOutputSchema>;

export async function extractData(input: ExtractDataInput): Promise<ExtractDataOutput> {
  return extractDataFlow(input);
}

const prompt = ai.definePrompt({
  name: 'extractDataPrompt',
  input: {schema: ExtractDataInputSchema},
  output: {schema: ExtractDataOutputSchema},
  prompt: `You are an expert at extracting structured data from text. The provided text is metadata from a file, potentially in a specific format like GUANO.

Your task is to parse this text and extract the relevant data points.

- The 'Loc Position' field contains both latitude and longitude, separated by a space. You must extract them into the separate 'latitude' and 'longitude' fields.
- The temperature might be labeled as 'Temperature Int'.
- If a field like 'flybys' is not present in the text, you should omit it from the output for that data point.
- If no parsable data is found, return an empty array.

Here is an example of the input format:
"GUANO|Version:1.0 ... Timestamp:2025-03-02 20:50:16+13:00 Loc Position:-37.00403 174.57577 Temperature Int:20.75"

From this example, you should extract:
- timestamp: "2025-03-02 20:50:16+13:00"
- latitude: -37.00403
- longitude: 174.57577
- temperature: 20.75

Now, please process the following content:

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
