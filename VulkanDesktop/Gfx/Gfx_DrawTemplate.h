#pragma once

#include "../RenderContract/Gpu_DrawTemplate.h"

#include "Gfx_DrawExtract.h"

// Fill one template from extract output + mesh index count (RenderCore resolves mesh buffers).
void Gfx_FillDrawTemplate( Gpu_DrawTemplate& aOut, const Gfx_DrawInstance& aDraw, uint32_t aIndexCount, uint32_t aInstanceDataOffset );
