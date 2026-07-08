#include <gtest/gtest.h>

#include "opentelemetry/exporters/memory/in_memory_span_exporter.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/sdk/trace/processor.h"
#include "opentelemetry/sdk/trace/recordable.h"
#include "opentelemetry/sdk/trace/simple_processor.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/trace/provider.h"
#include "pico_interface/StateEstimator.hpp"

namespace trace = opentelemetry::trace;
namespace sdktrace = opentelemetry::sdk::trace;

TEST(TelemetryTest, VerifySpanExported) {
  // 1. Setup the In-Memory Exporter
  auto memory_exporter =
      std::make_unique<opentelemetry::exporter::memory::InMemorySpanExporter>();
  // Keep a raw pointer to the concrete type
  auto *exporter_ptr = memory_exporter.get();

  // 2. Setup the Processor and Provider
  auto processor = std::unique_ptr<sdktrace::SpanProcessor>(
      new sdktrace::SimpleSpanProcessor(std::move(memory_exporter)));
  auto provider =
      std::make_unique<sdktrace::TracerProvider>(std::move(processor));

  // 3. Create a span
  auto tracer = provider->GetTracer("test_tracer");
  auto span = tracer->StartSpan("test_operation");
  span->End();

  // 4. Assert that the span is in the memory buffer
  auto span_data_collection = exporter_ptr->GetData();
  auto spans = span_data_collection->GetSpans();
  ASSERT_EQ(spans.size(), 1);
  EXPECT_EQ(spans[0]->GetName(), "test_operation");
}

TEST(TelemetryTest, VerifyStateEstimatorSpans) {
  // 1. Setup the In-Memory Exporter
  auto memory_exporter =
      std::make_unique<opentelemetry::exporter::memory::InMemorySpanExporter>();
  auto *exporter_ptr = memory_exporter.get();

  // 2. Setup the Processor and Provider with shared_ptr (required by
  // SetTracerProvider)
  auto processor = std::unique_ptr<sdktrace::SpanProcessor>(
      new sdktrace::SimpleSpanProcessor(std::move(memory_exporter)));
  auto provider = std::shared_ptr<trace::TracerProvider>(
      new sdktrace::TracerProvider(std::move(processor)));

  // Set the global tracer provider so StateEstimator can retrieve it
  trace::Provider::SetTracerProvider(provider);

  // 3. Setup StateEstimator with standard RobotConfig
  TelemetryQueue<EncoderIMUTelemetry> queue;
  RobotConfig config;
  config.wheel_radius = 0.033;
  config.wheel_base = 0.16;
  config.ticks_per_rev = 1440.0;
  config.scale_factor = 1.0;

  StateEstimator estimator(queue, config);

  // 4. Process telemetry packet #1 (initialization)
  EncoderIMUTelemetry tel1{};
  tel1.timestamp_us = 1000000;
  tel1.count_fl = 0;
  tel1.count_rl = 0;
  tel1.count_fr = 0;
  tel1.count_rr = 0;
  estimator.process_telemetry(tel1);

  // 5. Process telemetry packet #2 (EKF update step, 100ms later)
  EncoderIMUTelemetry tel2{};
  tel2.timestamp_us = 1100000;
  tel2.count_fl = 144;  // moving forward
  tel2.count_rl = 144;
  tel2.count_fr = 144;
  tel2.count_rr = 144;
  estimator.process_telemetry(tel2);

  // 6. Assert that the spans are in the memory buffer and verify their
  // attributes
  auto span_data_collection = exporter_ptr->GetData();
  auto spans = span_data_collection->GetSpans();
  ASSERT_EQ(spans.size(), 2);

  // Verify Initialization Span
  EXPECT_EQ(spans[0]->GetName(), "process_telemetry");

  // Verify EKF Update Span
  EXPECT_EQ(spans[1]->GetName(), "process_telemetry");

  // Reset global provider to avoid side effects in other tests
  trace::Provider::SetTracerProvider(std::shared_ptr<trace::TracerProvider>());
}
