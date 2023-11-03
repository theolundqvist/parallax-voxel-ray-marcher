#include <imgui.h>

#include "Log.h"
#include "LogView.h"

#ifdef _WIN32
#pragma warning (disable : 4996) // This function or variable may be unsafe
#endif

char Log::View::mOutput[BUFFER_ROWS * BUFFER_WIDTH * 2];
char Log::View::mBuffer[BUFFER_ROWS][BUFFER_WIDTH];
int Log::View::mLen[BUFFER_ROWS];
Log::Type Log::View::mType[BUFFER_ROWS];
int Log::View::mBufferPtr = 0;
bool Log::View::mAutoScroll = true;
bool Log::View::mScrollToBottom = true;
static ImVec4 logViewTypeColor[Log::N_TYPES];

void Log::View::Init()
{
	memset(mBuffer, 0, BUFFER_ROWS * BUFFER_WIDTH);
	for (int i = 0; i < BUFFER_ROWS; i++)
		mType[i] = Log::TYPE_TRIVIA;
	Log::SetCustomOutputTargetFunc(Feed);
	for (int i = 0; i < Log::N_TYPES; i++)
		logViewTypeColor[i] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // default color
	logViewTypeColor[Log::TYPE_WARNING	] = ImVec4(0.7f, 0.4f, 0.0f, 1.0f);
	logViewTypeColor[Log::TYPE_ERROR	] = ImVec4(0.7f, 0.0f, 0.0f, 1.0f);
	logViewTypeColor[Log::TYPE_ASSERT	] = ImVec4(0.7f, 0.0f, 0.0f, 1.0f);
	logViewTypeColor[Log::TYPE_PARAM	] = ImVec4(0.7f, 0.0f, 0.0f, 1.0f);
	logViewTypeColor[Log::TYPE_TRIVIA	] = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);

	mScrollToBottom = true;
}

void Log::View::Destroy()
{

}

void Log::View::Render()
{
	// Inspired by Dear ImGUI's ExampleAppConsole
	bool const isWindowExpended = ImGui::Begin("Log", nullptr, ImGuiWindowFlags_None);
	if (!isWindowExpended) {
		ImGui::End();
		return;
	}

	bool const copyToClipboard = ImGui::SmallButton("Copy"); ImGui::SameLine();
	mScrollToBottom = mScrollToBottom || ImGui::SmallButton("Scroll to bottom"); ImGui::SameLine();
	if (ImGui::SmallButton("Clear")) ClearLog();

	ImGui::Separator();

        // Options menu
        if (ImGui::BeginPopup("Options"))
        {
            ImGui::Checkbox("Auto-scroll", &mAutoScroll);
            ImGui::EndPopup();
        }

	static ImGuiTextFilter filter;
	filter.Draw("Filter (\"incl,-excl\")", 180);

	ImGui::Separator();

        // Reserve enough left-over height for 1 separator + 1 input text
        const float footerHeightToReserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footerHeightToReserve), false, ImGuiWindowFlags_HorizontalScrollbar);
	if (ImGui::BeginPopupContextWindow())
	{
		if (ImGui::Selectable("Clear")) ClearLog();
		ImGui::EndPopup();
	}

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
	if (copyToClipboard)
		ImGui::LogToClipboard();

	for (int i = 0; i < BUFFER_ROWS; i++) {
		int pos = (BUFFER_ROWS + (mBufferPtr + i)) % BUFFER_ROWS;
		if (mLen[pos] == 0 || !filter.PassFilter(mBuffer[pos]))
			continue;
		ImGui::PushStyleColor(ImGuiCol_Text, logViewTypeColor[mType[pos]]);
		ImGui::TextWrapped("%s", mBuffer[pos]);
		ImGui::PopStyleColor();
	}

	if (copyToClipboard)
		ImGui::LogFinish();
	if (mScrollToBottom || (mAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
            ImGui::SetScrollHereY(1.0f);
	mScrollToBottom = false;

	ImGui::PopStyleVar();
	ImGui::EndChild();

	ImGui::End();
}

void Log::View::Feed(Log::Type type, const char *msg)
{
	strncpy(mBuffer[mBufferPtr], msg, BUFFER_WIDTH - 1);
	mLen[mBufferPtr] = (int) strlen(msg);
	mType[mBufferPtr] = type;
	mBufferPtr = (mBufferPtr + 1) % BUFFER_ROWS;

	mScrollToBottom = true;
}

void Log::View::ClearLog()
{
	for (int& length : mLen)
		length = 0;
	mBufferPtr = 0;

	mScrollToBottom = true;
}
