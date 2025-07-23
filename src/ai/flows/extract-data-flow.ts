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
  timestamp: z.string().describe('The timestamp of the data point.'),
  latitude: z.number().describe('The latitude of the location.'),
  longitude: z.number().describe('The longitude of the location.'),
  temperature: z.number().describe('The temperature reading.'),
  flybys: z.number().describe('The number of fly-bys.'),
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
  prompt: `You are an expert at extracting structured data from text. The provided text may contain sensor readings and other data points. Please extract all available data points into a structured format. If no data is found, return an empty array.

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
