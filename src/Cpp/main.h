#pragma once

void RenderInitializingState();

void RenderMainMenuState();

void RenderRunningTaskState();

void RenderTaskSuccessState();

void RenderStoppingState();

bool maa_loop();

void UpdateInitializationProgress(float progress);
